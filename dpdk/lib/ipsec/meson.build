# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources = files('esp_inb.c', 'esp_outb.c', 'sa.c', 'ses.c', 'ipsec_sad.c')

headers = files('rte_ipsec.h', 'rte_ipsec_sa.h', 'rte_ipsec_sad.h')
indirect_headers += files('rte_ipsec_group.h')

deps += ['mbuf', 'net', 'cryptodev', 'security', 'hash']
build = false
reason = 'not needed by SPDK'
