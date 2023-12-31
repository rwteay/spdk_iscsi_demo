#
# Copyright (c) 2023, Intel Corporation
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
#

APP := imb-acvp

# include common options
include ../common/common.mk

ACVP_LOC ?= /usr/local/acvp
ACVP_HDR ?= $(ACVP_LOC)/include
ACVP_LIB ?= $(ACVP_LOC)/lib
ACVP_CFLAGS = -I$(ACVP_HDR) $(CFLAGS)
ACVP_LDFLAGS = -L$(ACVP_LIB) $(LDFLAGS)
ACVP_LDLIBS = -lacvp $(LDLIBS)

# acvp_app modules
ACVP_SOURCES := acvp_app_main.c utils.o

# targets come here
all: $(APP)

$(APP): $(ACVP_SOURCES)
ifneq ("$(wildcard $(ACVP_HDR))","")
	$(CC) $(ACVP_CFLAGS) $(ACVP_LDFLAGS) $^ $(ACVP_LDLIBS) -o $@
else
	@echo "No libacvp header found at $(ACVP_HDR), $(APP) is not built"
endif

utils.o: ../common/utils.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	-rm -f $(DEP_FILES) $(APP) utils.o

# if target not clean then make dependencies
ifneq ($(MAKECMDGOALS),clean)
-include $(wildcard *.d)
endif
