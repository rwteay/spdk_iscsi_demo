#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#

SPDK_ROOT_DIR := $(abspath $(CURDIR)/../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

DIRS-y =

ifeq ($(OS),Linux)
# Tests in this directory mostly depend upon aio bdevs, which are not widely supported.
DIRS-y += esnap
endif

.PHONY: all clean $(DIRS-y)

all: $(DIRS-y)
clean: $(DIRS-y)

include $(SPDK_ROOT_DIR)/mk/spdk.subdirs.mk
