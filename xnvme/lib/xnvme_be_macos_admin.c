// SPDX-FileCopyrightText: Samsung Electronics Co., Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <libxnvme.h>
#include <xnvme_be.h>
#include <xnvme_be_nosys.h>
#ifdef XNVME_BE_MACOS_ENABLED
#include <mach/mach_error.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/storage/nvme/NVMeSMARTLibExternal.h>

#include <xnvme_dev.h>
#include <xnvme_be_macos.h>

int
_gfeat(struct xnvme_cmd_ctx *ctx, void *XNVME_UNUSED(dbuf))
{
	struct xnvme_spec_feat feat = {0};

	switch (ctx->cmd.gfeat.cdw10.fid) {
	case XNVME_SPEC_FEAT_NQUEUES:
		feat.nqueues.nsqa = 63;
		feat.nqueues.ncqa = 63;
		ctx->cpl.cdw0 = feat.val;
		break;

	default:
		XNVME_DEBUG("FAILED: unsupported fid: %d", ctx->cmd.gfeat.cdw10.fid);
		return -ENOSYS;
	}

	return 0;
}

int
xnvme_be_macos_admin(struct xnvme_cmd_ctx *ctx, void *dbuf, size_t dbuf_nbytes,
		     void *XNVME_UNUSED(mbuf), size_t XNVME_UNUSED(mbuf_nbytes))
{
	IOReturn err;
	int nsid;
	struct xnvme_be_macos_state *state = (void *)ctx->dev->be.state;

	switch (ctx->cmd.common.opcode) {
	case XNVME_SPEC_ADM_OPC_LOG:
		err = (*state->nvme_smart_interface)
			      ->GetLogPage(state->nvme_smart_interface, dbuf, ctx->cmd.log.lid,
					   dbuf_nbytes / 4 - 1);
		if (err != kIOReturnSuccess) {
			XNVME_DEBUG("Error in GetLogPage: %s", (char *)mach_error_string(err));
			return 1;
		}
		break;
	case XNVME_SPEC_ADM_OPC_IDFY:
		nsid = 0; // 0 for controller identify data as per Apple docs
		if (ctx->cmd.idfy.cns == 0) {
			// If we are providing a namespace id, overwrite with that.
			nsid = ctx->cmd.common.nsid;
		}
		err = (*state->nvme_smart_interface)
			      ->GetIdentifyData(state->nvme_smart_interface, dbuf, nsid);
		if (err != kIOReturnSuccess) {
			XNVME_DEBUG("Error in GetIdentifyData: %s",
				    (char *)mach_error_string(err));
			return 1;
		}
		break;
	case XNVME_SPEC_ADM_OPC_GFEAT:
		return _gfeat(ctx, dbuf);
	default:
		XNVME_DEBUG("FAILED: ENOSYS opcode: %d", ctx->cmd.common.opcode);
		return -ENOSYS;
	}

	return 0;
}

#endif

struct xnvme_be_admin g_xnvme_be_macos_admin = {
	.id = "nvme",
#ifdef XNVME_BE_MACOS_ENABLED
	.cmd_admin = xnvme_be_macos_admin,
	.cmd_pseudo = xnvme_be_nosys_sync_cmd_pseudo,
#else
	.cmd_admin = xnvme_be_nosys_sync_cmd_admin,
	.cmd_pseudo = xnvme_be_nosys_sync_cmd_pseudo,
#endif
};
