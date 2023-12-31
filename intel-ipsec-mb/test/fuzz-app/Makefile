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

FUZZ_APP := imb-fuzz-api
DIRECT_FUZZ_APP := imb-fuzz-direct-api

# include common options
include ../common/common.mk

FUZZ_CFLAGS = -I../../lib -L../../lib $(CFLAGS) -g -fsanitize=fuzzer,address,leak

# fuzz modules
FUZZ_SOURCES := job_api_fuzz_test.c
DIRECT_FUZZ_SOURCES := direct_api_fuzz_test.c

# targets come here
all: $(FUZZ_APP) $(DIRECT_FUZZ_APP)

$(FUZZ_APP): $(FUZZ_SOURCES)
ifeq ($(MINGW),0)
ifneq (, $(shell which clang))
	clang $(FUZZ_CFLAGS) $(FUZZ_SOURCES) $(LDLIBS) -o $@
else
	@echo "Clang is not installed. $(FUZZ_APP) is not built"
endif
else
	@echo "Fuzzing not supported on MinGW build. $(FUZZ_APP) is not built"
endif

$(DIRECT_FUZZ_APP): $(DIRECT_FUZZ_SOURCES)
ifeq ($(MINGW),0)
ifneq (, $(shell which clang))
	clang $(FUZZ_CFLAGS) $(DIRECT_FUZZ_SOURCES) $(LDLIBS) -o $@
else
	@echo "Clang is not installed. $(DIRECT_FUZZ_APP) is not built"
endif
else
	@echo "Fuzzing not supported on MinGW build. $(DIRECT_FUZZ_APP) is not built"
endif

.PHONY: clean
clean:
	-rm -f $(DEP_FILES) $(FUZZ_APP) $(DIRECT_FUZZ_APP)

# if target not clean then make dependencies
ifneq ($(MAKECMDGOALS),clean)
-include $(wildcard *.d)
endif
