# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

qat_crypto = true
qat_crypto_path = 'crypto/qat'
qat_crypto_relpath = '../../' + qat_crypto_path
qat_compress = true
qat_compress_path = 'compress/qat'
qat_compress_relpath = '../../' + qat_compress_path

if disable_drivers.contains(qat_crypto_path)
    qat_crypto = false
    dpdk_drvs_disabled += qat_crypto_path
    set_variable(qat_crypto_path.underscorify() + '_disable_reason',
            'Explicitly disabled via build config')
endif
if disable_drivers.contains(qat_compress_path)
    qat_compress = false
    dpdk_drvs_disabled += qat_compress_path
    set_variable(qat_compress_path.underscorify() + '_disable_reason',
            'Explicitly disabled via build config')
endif

libcrypto = dependency('libcrypto', required: false, method: 'pkg-config')
if qat_crypto and not libcrypto.found()
    qat_crypto = false
    dpdk_drvs_disabled += qat_crypto_path
    set_variable(qat_crypto_path.underscorify() + '_disable_reason',
            'missing dependency, libcrypto')
endif

# The driver should not build if both compression and crypto are disabled
#FIXME common code depends on compression files so check only compress!
if not qat_compress # and not qat_crypto
    build = false
    reason = '' # rely on reason for compress/crypto above
    subdir_done()
endif

deps += ['bus_pci', 'cryptodev', 'net', 'compressdev']
sources += files(
        'qat_common.c',
        'qat_qp.c',
        'qat_device.c',
        'qat_logs.c',
)
includes += include_directories(
        'qat_adf',
        qat_crypto_relpath,
        qat_compress_relpath,
)

if qat_compress
    foreach f: ['qat_comp_pmd.c', 'qat_comp.c']
        sources += files(join_paths(qat_compress_relpath, f))
    endforeach
endif

if qat_crypto
    foreach f: ['qat_sym_pmd.c', 'qat_sym.c', 'qat_sym_session.c',
            'qat_sym_hw_dp.c']
        sources += files(join_paths(qat_crypto_relpath, f))
    endforeach
    deps += ['security']
    ext_deps += libcrypto
    cflags += ['-DBUILD_QAT_SYM']
endif
