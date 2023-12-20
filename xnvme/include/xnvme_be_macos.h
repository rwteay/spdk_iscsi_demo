// SPDX-FileCopyrightText: Samsung Electronics Co., Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __INTERNAL_XNVME_BE_MACOS_H
#define __INTERNAL_XNVME_BE_MACOS_H

/**
 * Internal representation of XNVME_BE_MACOS state
 *
 * NOTE: When changing this struct, ensure compatibility with 'struct xnvme_be_cbi_state'
 */
struct xnvme_be_macos_state {
	int fd;
	io_object_t ioservice_device;
	IONVMeSMARTInterface **nvme_smart_interface;
	IOCFPlugInInterface **plugin_interface;

	uint8_t _rsvd[104];
};
XNVME_STATIC_ASSERT(sizeof(struct xnvme_be_macos_state) == XNVME_BE_STATE_NBYTES, "Incorrect size")

extern struct xnvme_be_admin g_xnvme_be_macos_admin;
extern struct xnvme_be_sync g_xnvme_be_macos_sync_psync;
extern struct xnvme_be_dev g_xnvme_be_macos_dev;

#endif /* __INTERNAL_XNVME_BE_MACOS */
