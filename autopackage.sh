#!/usr/bin/env bash
#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (C) 2015 Intel Corporation
#  All rights reserved.
#

rootdir=$(readlink -f $(dirname $0))
source "$rootdir/test/common/autobuild_common.sh"

MAKEFLAGS=${MAKEFLAGS:--j16}
cd $rootdir

if [[ $SPDK_TEST_RELEASE_BUILD -eq 1 ]]; then
	build_packaging
	$MAKE clean
fi

if [[ $RUN_NIGHTLY -eq 0 || $SPDK_TEST_UNITTEST -eq 0 ]]; then
	timing_finish
	exit 0
fi

timing_enter build_release

config_params="$(get_config_params | sed 's/--enable-debug//g')"
if [ $(uname -s) = Linux ]; then
	# LTO needs a special compiler to work under clang. See detect_cc.sh for details.
	if [[ $CC == *clang* ]]; then
		LD=$(type -P ld.gold)
		export LD
	fi
	$rootdir/configure $config_params --enable-lto
else
	# LTO needs a special compiler to work on BSD.
	$rootdir/configure $config_params
fi
$MAKE ${MAKEFLAGS}
$MAKE ${MAKEFLAGS} clean

timing_exit build_release

timing_finish
