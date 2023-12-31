# Copyright (c) 2017-2023, Intel Corporation
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

EXE=imb-perf
INSTPATH ?= /usr/include/intel-ipsec-mb.h
LIB_DIR ?= ../lib
NASM ?= nasm

MINGW ?= $(shell $(CC) -dM -E - < /dev/null | grep -i mingw | wc -l | sed 's/^ *//')

CFLAGS = -D_GNU_SOURCE $(INCLUDES) \
	-W -Wall -Wextra -Wmissing-declarations -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings  \
	-Wformat -Wformat-security \
	-Wunreachable-code -Wmissing-noreturn -Wsign-compare -Wno-endif-labels \
	-Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition \
	-pthread -fno-delete-null-pointer-checks -fwrapv -std=c99

# -fno-strict-overflow is not supported by clang
ifneq ($(CC),clang)
CFLAGS += -fno-strict-overflow
endif

ifeq ($(MINGW),0)
CFLAGS += -DLINUX
NASM_FLAGS := -Werror -felf64 -Xgnu -gdwarf -DLINUX -D__linux__
else
NASM_FLAGS := -Werror -fwin64 -Xvc -gcv8 -DWIN_ABI
endif

# if "-z ibt" is supported then assume "-z shstk, -z cet-report=error" are also supported
# "-fcf-protection" needs to be checked separately
ifeq ($(MINGW),0)
CC_HAS_CET = $(and $(shell $(CC) --target-help 2> /dev/null | grep -m1 -e "-z ibt" | wc -l), \
	$(shell $(CC) --help=common 2> /dev/null | grep -m1 -e "-fcf-protection" | wc -l))
endif
ifeq ($(CC_HAS_CET),1)
CFLAGS += -fcf-protection=full
endif

ifeq ($(MINGW),0)
LDFLAGS = -fPIE -z noexecstack -z relro -z now -pthread
endif
ifeq ($(CC_HAS_CET),1)
LDFLAGS += -Wl,-z,ibt -Wl,-z,shstk -Wl,-z,cet-report=error
endif
LDLIBS = -lIPSec_MB

ifeq ("$(shell test -e $(INSTPATH) && echo -n yes)","yes")
# library installed
CFLAGS +=
else
# library not installed
CFLAGS += -I../lib
LDFLAGS += -L$(LIB_DIR)
endif

DEBUG_OPT ?= -O0
ifeq ($(DEBUG),y)
CFLAGS += -g -DDEBUG $(DEBUG_OPT)
LDFLAGS += -g
else
ifeq ($(MINGW),0)
CFLAGS += -O3 -fPIE -fstack-protector -D_FORTIFY_SOURCE=2
else
CFLAGS += -O2 -fPIE
endif
endif

SOURCES := ipsec_perf.c msr.c
ASM_SOURCES := misc.asm
OBJECTS := $(SOURCES:%.c=%.o) $(ASM_SOURCES:%.asm=%.o)
DEPFILES := $(OBJECTS:%.o=%.d)

CPPCHECK ?= cppcheck

.PHONY: all clean style cppcheck

# rule for compiling assembly code with producing dependencies
%.o:%.asm
	$(NASM) -MD $(@:.o=.d) -MT $@ -o $@ $(NASM_FLAGS) $<

all: $(EXE)

$(EXE): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	-rm -f $(OBJECTS)
	-rm -f $(DEPFILES)
	-rm -f $(EXE)

.PHONY: style
style: clang-format checkpatch

.PHONY: clang-format
CLANGFORMAT?=clang-format
clang-format:
	@for file in $(wildcard ./*.[ch]) $(wildcard ./**/*.[ch]); do \
		echo "Checking style $$file"; \
		$(CLANGFORMAT) -style=file "$$file" | diff "$$file" - | tee /dev/stderr | [ $$(wc -c) -eq 0 ] || \
		{ echo "ERROR: $$file has style problems"; exit 1; } \
	done

.PHONY: checkpatch
SOURCES_STYLE := $(foreach infile,$(ASM_SOURCES),-f $(infile))
CHECKPATCH?=checkpatch.pl
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --emacs --no-color \
--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE,SPLIT_STRING,\
UNSPECIFIED_INT,ARRAY_SIZE,BLOCK_COMMENT_STYLE,GLOBAL_INITIALISERS,\
COMPLEX_MACRO,SPACING,STORAGE_CLASS,USE_FUNC,NEW_TYPEDEFS,VOLATILE,\
CONSTANT_COMPARISON --max-line-length=100 $(SOURCES_STYLE)

# cppcheck analysis check
CPPCHECK ?= cppcheck
CPPCHECK_OPTS ?= -I../lib -I./
CPPCHECK_FLAGS ?= -j $(shell getconf _NPROCESSORS_ONLN)
CPPCHECK_FLAGS1 ?= --cppcheck-build-dir=.cppcheck $(CPPCHECK_FLAGS)
CPPCHECK_FLAGS2 ?= --cppcheck-build-dir=.bughunt $(CPPCHECK_FLAGS)

.PHONY: cppcheck
cppcheck:
	mkdir -p .cppcheck
	$(CPPCHECK) --force --enable=all $(CPPCHECK_FLAGS1) $(CPPCHECK_OPTS) ./

.PHONY: bughunt
bughunt:
	mkdir -p .bughunt
	$(CPPCHECK) --bug-hunting --inconclusive $(CPPCHECK_FLAGS2) $(CPPCHECK_OPTS) ./
