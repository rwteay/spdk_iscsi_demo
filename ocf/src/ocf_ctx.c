/*
 * Copyright(c) 2012-2022 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ocf/ocf.h"
#include "ocf_ctx_priv.h"
#include "ocf_priv.h"
#include "ocf_volume_priv.h"
#include "ocf_request.h"
#include "ocf_logger_priv.h"
#include "ocf_core_priv.h"
#include "ocf_cache_priv.h"
#include "ocf_composite_volume_priv.h"
#include "mngt/ocf_mngt_core_pool_priv.h"
#include "metadata/metadata_io.h"

/*
 *
 */
int ocf_ctx_register_volume_type_internal(ocf_ctx_t ctx, uint8_t type_id,
		const struct ocf_volume_properties *properties,
		const struct ocf_volume_extended *extended)
{
	int result = 0;

	if (!ctx || !properties)
		return -EINVAL;

	env_rmutex_lock(&ctx->lock);

	if (type_id >= OCF_VOLUME_TYPE_MAX || ctx->volume_type[type_id]) {
		env_rmutex_unlock(&ctx->lock);
		result = -EINVAL;
		goto err;
	}

	ocf_volume_type_init(&ctx->volume_type[type_id], properties, extended);
	if (!ctx->volume_type[type_id])
		result = -EINVAL;

	env_rmutex_unlock(&ctx->lock);

	if (result)
		goto err;

	ocf_log(ctx, log_debug, "'%s' volume operations registered\n",
			properties->name);
	return 0;

err:
	ocf_log(ctx, log_err, "Failed to register volume operations '%s'\n",
			properties->name);
	return result;
}

int ocf_ctx_register_volume_type(ocf_ctx_t ctx, uint8_t type_id,
		const struct ocf_volume_properties *properties)
{
	if (type_id >= OCF_VOLUME_TYPE_MAX_USER)
		return -EINVAL;

	return ocf_ctx_register_volume_type_internal(ctx, type_id,
			properties, NULL);
}

/*
 *
 */
void ocf_ctx_unregister_volume_type_internal(ocf_ctx_t ctx, uint8_t type_id)
{
	OCF_CHECK_NULL(ctx);

	env_rmutex_lock(&ctx->lock);

	if (type_id < OCF_VOLUME_TYPE_MAX && ctx->volume_type[type_id]) {
		ocf_volume_type_deinit(ctx->volume_type[type_id]);
		ctx->volume_type[type_id] = NULL;
	}

	env_rmutex_unlock(&ctx->lock);
}

void ocf_ctx_unregister_volume_type(ocf_ctx_t ctx, uint8_t type_id)
{
	OCF_CHECK_NULL(ctx);

	if (type_id < OCF_VOLUME_TYPE_MAX)
		ocf_ctx_unregister_volume_type_internal(ctx, type_id);
}

/*
 *
 */
ocf_volume_type_t ocf_ctx_get_volume_type_internal(ocf_ctx_t ctx,
		uint8_t type_id)
{
	ocf_volume_type_t volume_type;

	OCF_CHECK_NULL(ctx);

	if (type_id >= OCF_VOLUME_TYPE_MAX)
		return NULL;

	env_rmutex_lock(&ctx->lock);
	volume_type = ctx->volume_type[type_id];
	env_rmutex_unlock(&ctx->lock);

	return volume_type;
}

ocf_volume_type_t ocf_ctx_get_volume_type(ocf_ctx_t ctx, uint8_t type_id)
{
	OCF_CHECK_NULL(ctx);

	if (type_id >= OCF_VOLUME_TYPE_MAX_USER)
		return NULL;

	return ocf_ctx_get_volume_type_internal(ctx, type_id);
}

/*
 *
 */
int ocf_ctx_get_volume_type_id(ocf_ctx_t ctx, ocf_volume_type_t type)
{
	int i;

	OCF_CHECK_NULL(ctx);

	env_rmutex_lock(&ctx->lock);
	for (i = 0; i < OCF_VOLUME_TYPE_MAX; ++i) {
		if (ctx->volume_type[i] == type)
			break;
	}
	env_rmutex_unlock(&ctx->lock);

	return (i < OCF_VOLUME_TYPE_MAX) ? i : -1;
}

/*
 *
 */
int ocf_ctx_volume_create(ocf_ctx_t ctx, ocf_volume_t *volume,
		struct ocf_volume_uuid *uuid, uint8_t type_id)
{
	ocf_volume_type_t volume_type;

	OCF_CHECK_NULL(ctx);

	volume_type = ocf_ctx_get_volume_type(ctx, type_id);
	if (!volume_type)
		return -EINVAL;

	return ocf_volume_create(volume, volume_type, uuid);
}

static void check_ops_provided(const struct ocf_ctx_ops *ops)
{
	ENV_BUG_ON(!ops->data.alloc);
	ENV_BUG_ON(!ops->data.free);
	ENV_BUG_ON(!ops->data.mlock);
	ENV_BUG_ON(!ops->data.munlock);
	ENV_BUG_ON(!ops->data.read);
	ENV_BUG_ON(!ops->data.write);
	ENV_BUG_ON(!ops->data.zero);
	ENV_BUG_ON(!ops->data.seek);
	ENV_BUG_ON(!ops->data.copy);
	ENV_BUG_ON(!ops->data.secure_erase);

	ENV_BUG_ON(!ops->cleaner.init);
	ENV_BUG_ON(!ops->cleaner.kick);
	ENV_BUG_ON(!ops->cleaner.stop);
}

/*
 *
 */
int ocf_ctx_create(ocf_ctx_t *ctx, const struct ocf_ctx_config *cfg)
{
	ocf_ctx_t ocf_ctx;
	int ret;

	OCF_CHECK_NULL(ctx);
	OCF_CHECK_NULL(cfg);

	check_ops_provided(&cfg->ops);

	ocf_ctx = env_zalloc(sizeof(*ocf_ctx), ENV_MEM_NORMAL);
	if (!ocf_ctx)
		return -ENOMEM;

	INIT_LIST_HEAD(&ocf_ctx->caches);
	env_atomic_set(&ocf_ctx->ref_count, 1);
	ret = env_rmutex_init(&ocf_ctx->lock);
	if (ret)
		goto err_ctx;

	ocf_ctx->ops = &cfg->ops;
	ocf_ctx->cfg = cfg;

	ocf_logger_init(&ocf_ctx->logger, &cfg->ops.logger, cfg->logger_priv);

	ret = ocf_logger_open(&ocf_ctx->logger);
	if (ret)
		goto err_ctx;

	ret = ocf_req_allocator_init(ocf_ctx);
	if (ret)
		goto err_logger;

	ret = ocf_metadata_io_ctx_init(ocf_ctx);
	if (ret)
		goto err_mio;

	ret = ocf_core_volume_type_init(ocf_ctx);
	if (ret)
		goto err_utils;

	ret = ocf_cache_volume_type_init(ocf_ctx);
	if (ret)
		goto err_utils;

	ret = ocf_composite_volume_type_init(ocf_ctx);
	if (ret)
		goto err_core_volume;

	ocf_mngt_core_pool_init(ocf_ctx);

	*ctx = ocf_ctx;

	return 0;

err_core_volume:
	ocf_ctx_unregister_volume_type(ocf_ctx, OCF_VOLUME_TYPE_CORE);
err_utils:
	ocf_metadata_io_ctx_deinit(ocf_ctx);
err_mio:
	ocf_req_allocator_deinit(ocf_ctx);
err_logger:
	ocf_logger_close(&ocf_ctx->logger);
err_ctx:
	env_free(ocf_ctx);
	return ret;
}

/*
 *
 */
void ocf_ctx_get(ocf_ctx_t ctx)
{
	OCF_CHECK_NULL(ctx);

	env_atomic_inc(&ctx->ref_count);
}

/*
 *
 */
static void ocf_ctx_unregister_volume_types(ocf_ctx_t ctx)
{
	int id;

	for (id = 0; id < OCF_VOLUME_TYPE_MAX; id++)
		ocf_ctx_unregister_volume_type(ctx, id);
}

/*
 *
 */
void ocf_ctx_put(ocf_ctx_t ctx)
{
	OCF_CHECK_NULL(ctx);

	if (env_atomic_dec_return(&ctx->ref_count))
		return;

	env_rmutex_lock(&ctx->lock);
	ENV_BUG_ON(!list_empty(&ctx->caches));
	env_rmutex_unlock(&ctx->lock);

	ocf_mngt_core_pool_deinit(ctx);
	ocf_ctx_unregister_volume_types(ctx);
	env_rmutex_destroy(&ctx->lock);

	ocf_metadata_io_ctx_deinit(ctx);
	ocf_req_allocator_deinit(ctx);
	ocf_logger_close(&ctx->logger);
	env_free(ctx);
}
