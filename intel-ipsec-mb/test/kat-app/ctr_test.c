/*****************************************************************************
 Copyright (c) 2017-2023, Intel Corporation

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <intel-ipsec-mb.h>
#include "utils.h"
#include "cipher_test.h"

#define MAX_CTR_JOBS 32

#define BYTE_ROUND_UP(x) ((x + 7) / 8)

int
ctr_test(struct IMB_MGR *);

extern const struct cipher_test ctr_test_json[];
extern const struct cipher_test ctr_bit_test_json[];

static int
test_ctr(struct IMB_MGR *mb_mgr, const void *expkey, unsigned key_len, const void *iv,
         unsigned iv_len, const uint8_t *in_text, const uint8_t *out_text, unsigned text_len,
         const IMB_CIPHER_DIRECTION dir, const IMB_CHAIN_ORDER order, const IMB_CIPHER_MODE alg)
{
        uint32_t text_byte_len;
        struct IMB_JOB *job;
        uint8_t padding[16];
        uint8_t *target;
        int ret = -1;

        /* Get number of bytes (in case algo is CNTR_BITLEN) */
        if (alg == IMB_CIPHER_CNTR)
                text_byte_len = text_len / 8;
        else
                text_byte_len = BYTE_ROUND_UP(text_len);

        target = malloc(text_byte_len + (sizeof(padding) * 2));
        if (target == NULL) {
                fprintf(stderr, "Can't allocate buffer memory\n");
                goto end;
        }

        memset(target, -1, text_byte_len + (sizeof(padding) * 2));
        memset(padding, -1, sizeof(padding));

        while (IMB_FLUSH_JOB(mb_mgr) != NULL)
                ;

        job = IMB_GET_NEXT_JOB(mb_mgr);
        job->cipher_direction = dir;
        job->chain_order = order;
        job->dst = target + 16;
        job->src = in_text;
        job->cipher_mode = alg;
        job->enc_keys = expkey;
        job->dec_keys = expkey;
        job->key_len_in_bytes = key_len;
        job->iv = iv;
        job->iv_len_in_bytes = iv_len;
        job->cipher_start_src_offset_in_bytes = 0;
        if (alg == IMB_CIPHER_CNTR)
                job->msg_len_to_cipher_in_bytes = text_byte_len;
        else
                job->msg_len_to_cipher_in_bits = text_len;

        job->hash_alg = IMB_AUTH_NULL;

        job = IMB_SUBMIT_JOB(mb_mgr);
        if (!job) {
                const int err = imb_get_errno(mb_mgr);

                printf("%d Unexpected null return from submit_job()\n"
                       "\t Error code %d, %s\n",
                       __LINE__, err, imb_get_strerror(err));
                goto end;
        }
        if (job->status != IMB_STATUS_COMPLETED) {
                const int err = imb_get_errno(mb_mgr);

                printf("%d job status: %d, error code %d, %s\n", __LINE__, job->status, err,
                       imb_get_strerror(err));
                goto end;
        }
        job = IMB_FLUSH_JOB(mb_mgr);
        if (job) {
                printf("%d Unexpected return from flush_job\n", __LINE__);
                goto end;
        }

        if (memcmp(out_text, target + 16, text_byte_len)) {
                printf("mismatched\n");
                hexdump(stderr, "Target", target + 16, text_byte_len);
                hexdump(stderr, "Expected", out_text, text_byte_len);
                goto end;
        }
        if (memcmp(padding, target, sizeof(padding))) {
                printf("overwrite head\n");
                hexdump(stderr, "Target", target, text_byte_len + 32);
                goto end;
        }
        if (memcmp(padding, target + sizeof(padding) + text_byte_len, sizeof(padding))) {
                printf("overwrite tail\n");
                hexdump(stderr, "Target", target, text_byte_len + 32);
                goto end;
        }
        ret = 0;
        while (IMB_FLUSH_JOB(mb_mgr) != NULL)
                ;
end:
        if (target != NULL)
                free(target);
        return ret;
}

static int
test_ctr_burst(struct IMB_MGR *mb_mgr, const void *expkey, unsigned key_len, const void *iv,
               unsigned iv_len, const uint8_t *in_text, const uint8_t *out_text, unsigned text_len,
               const IMB_CIPHER_DIRECTION dir, const IMB_CHAIN_ORDER order,
               const IMB_CIPHER_MODE alg, const uint32_t num_jobs)
{
        uint32_t text_byte_len, i, completed_jobs, jobs_rx = 0;
        struct IMB_JOB *job, *jobs[MAX_CTR_JOBS];
        uint8_t padding[16];
        uint8_t **targets = malloc(num_jobs * sizeof(void *));
        int ret = -1;

        if (targets == NULL)
                goto end_alloc;

        /* Get number of bytes (in case algo is CNTR_BITLEN) */
        if (alg == IMB_CIPHER_CNTR)
                text_byte_len = text_len / 8;
        else
                text_byte_len = BYTE_ROUND_UP(text_len);

        memset(targets, 0, num_jobs * sizeof(void *));
        memset(padding, -1, sizeof(padding));

        for (i = 0; i < num_jobs; i++) {
                targets[i] = malloc(text_byte_len + (sizeof(padding) * 2));
                if (targets[i] == NULL)
                        goto end_alloc;
                memset(targets[i], -1, text_byte_len + (sizeof(padding) * 2));
        }

        while (IMB_GET_NEXT_BURST(mb_mgr, num_jobs, jobs) < num_jobs)
                IMB_FLUSH_BURST(mb_mgr, num_jobs, jobs);

        for (i = 0; i < num_jobs; i++) {
                job = jobs[i];
                job->cipher_direction = dir;
                job->chain_order = order;
                job->dst = targets[i] + sizeof(padding);
                job->src = in_text;
                job->cipher_mode = alg;
                job->enc_keys = expkey;
                job->dec_keys = expkey;
                job->key_len_in_bytes = key_len;
                job->iv = iv;
                job->iv_len_in_bytes = iv_len;
                job->cipher_start_src_offset_in_bytes = 0;
                if (alg == IMB_CIPHER_CNTR)
                        job->msg_len_to_cipher_in_bytes = text_byte_len;
                else
                        job->msg_len_to_cipher_in_bits = text_len;
                job->hash_alg = IMB_AUTH_NULL;
                job->user_data = targets[i];
                job->user_data2 = (void *) ((uint64_t) i);
                imb_set_session(mb_mgr, job);
        }

        completed_jobs = IMB_SUBMIT_BURST(mb_mgr, num_jobs, jobs);
        if (completed_jobs != num_jobs) {
                int err = imb_get_errno(mb_mgr);

                if (err != 0) {
                        printf("submit_burst error %d : '%s'\n", err, imb_get_strerror(err));
                        goto end;
                } else {
                        printf("submit_burst error: not enough "
                               "jobs returned!\n");
                        goto end;
                }
        }

        for (i = 0; i < num_jobs; i++) {
                job = jobs[i];

                if (job->status != IMB_STATUS_COMPLETED) {
                        printf("job %u status not complete!\n", i + 1);
                        goto end;
                }
                if (memcmp(out_text, targets[i] + sizeof(padding), text_byte_len)) {
                        printf("mismatched\n");
                        hexdump(stderr, "Target", targets[i] + sizeof(padding), text_byte_len);
                        hexdump(stderr, "Expected", out_text, text_byte_len);
                        goto end;
                }
                if (memcmp(padding, targets[i], sizeof(padding))) {
                        printf("overwrite head\n");
                        hexdump(stderr, "Target", targets[i],
                                text_byte_len + (sizeof(padding) * 2));
                        goto end;
                }
                if (memcmp(padding, targets[i] + sizeof(padding) + text_byte_len,
                           sizeof(padding))) {
                        printf("overwrite tail\n");
                        hexdump(stderr, "Target", targets[i],
                                text_byte_len + (sizeof(padding) * 2));
                        goto end;
                }
                jobs_rx++;
        }

        if (jobs_rx != num_jobs) {
                printf("Expected %u jobs, received %u\n", num_jobs, jobs_rx);
                goto end;
        }
        ret = 0;
end:

end_alloc:
        if (targets != NULL) {
                for (i = 0; i < num_jobs; i++)
                        free(targets[i]);
                free(targets);
        }

        return ret;
}

static int
test_ctr_cipher_burst(struct IMB_MGR *mb_mgr, const void *expkey, unsigned key_len, const void *iv,
                      unsigned iv_len, const uint8_t *in_text, const uint8_t *out_text,
                      unsigned text_len, const IMB_CIPHER_DIRECTION dir,
                      const IMB_CHAIN_ORDER order, const IMB_CIPHER_MODE alg,
                      const uint32_t num_jobs)
{
        uint32_t text_byte_len, i, completed_jobs, jobs_rx = 0;
        struct IMB_JOB *job, jobs[MAX_CTR_JOBS];
        uint8_t padding[16];
        uint8_t **targets = malloc(num_jobs * sizeof(void *));
        int ret = -1;

        if (targets == NULL)
                goto end_alloc;

        /* Get number of bytes (in case algo is CNTR_BITLEN) */
        if (alg == IMB_CIPHER_CNTR)
                text_byte_len = text_len / 8;
        else
                text_byte_len = BYTE_ROUND_UP(text_len);

        memset(targets, 0, num_jobs * sizeof(void *));
        memset(padding, -1, sizeof(padding));

        for (i = 0; i < num_jobs; i++) {
                targets[i] = malloc(text_byte_len + (sizeof(padding) * 2));
                if (targets[i] == NULL)
                        goto end_alloc;
                memset(targets[i], -1, text_byte_len + (sizeof(padding) * 2));
        }

        for (i = 0; i < num_jobs; i++) {
                job = &jobs[i];
                job->cipher_direction = dir;
                job->chain_order = order;
                job->dst = targets[i] + sizeof(padding);
                job->src = in_text;
                job->cipher_mode = alg;
                job->enc_keys = expkey;
                job->dec_keys = expkey;
                job->key_len_in_bytes = key_len;
                job->iv = iv;
                job->iv_len_in_bytes = iv_len;
                job->cipher_start_src_offset_in_bytes = 0;
                if (alg == IMB_CIPHER_CNTR)
                        job->msg_len_to_cipher_in_bytes = text_byte_len;
                else
                        job->msg_len_to_cipher_in_bits = text_len;
                job->hash_alg = IMB_AUTH_NULL;
                job->user_data = targets[i];
                job->user_data2 = (void *) ((uint64_t) i);
        }

        completed_jobs = IMB_SUBMIT_CIPHER_BURST(mb_mgr, jobs, num_jobs, alg, dir, key_len);
        if (completed_jobs != num_jobs) {
                int err = imb_get_errno(mb_mgr);

                if (err != 0) {
                        printf("submit_burst error %d : '%s'\n", err, imb_get_strerror(err));
                        goto end;
                } else {
                        printf("submit_burst error: not enough "
                               "jobs returned!\n");
                        goto end;
                }
        }

        for (i = 0; i < num_jobs; i++) {
                job = &jobs[i];

                if (job->status != IMB_STATUS_COMPLETED) {
                        printf("job %u status not complete!\n", i + 1);
                        goto end;
                }
                if (memcmp(out_text, targets[i] + sizeof(padding), text_byte_len)) {
                        printf("mismatched\n");
                        hexdump(stderr, "Target", targets[i] + sizeof(padding), text_byte_len);
                        hexdump(stderr, "Expected", out_text, text_byte_len);
                        goto end;
                }
                if (memcmp(padding, targets[i], sizeof(padding))) {
                        printf("overwrite head\n");
                        hexdump(stderr, "Target", targets[i],
                                text_byte_len + (sizeof(padding) * 2));
                        goto end;
                }
                if (memcmp(padding, targets[i] + sizeof(padding) + text_byte_len,
                           sizeof(padding))) {
                        printf("overwrite tail\n");
                        hexdump(stderr, "Target", targets[i],
                                text_byte_len + (sizeof(padding) * 2));
                        goto end;
                }
                jobs_rx++;
        }

        if (jobs_rx != num_jobs) {
                printf("Expected %u jobs, received %u\n", num_jobs, jobs_rx);
                goto end;
        }
        ret = 0;
end:

end_alloc:
        if (targets != NULL) {
                for (i = 0; i < num_jobs; i++)
                        free(targets[i]);
                free(targets);
        }

        return ret;
}

static void
test_ctr_vectors(struct IMB_MGR *mb_mgr, struct test_suite_context *ctx128,
                 struct test_suite_context *ctx192, struct test_suite_context *ctx256,
                 const struct cipher_test *v, const IMB_CIPHER_MODE alg)
{
        DECLARE_ALIGNED(uint32_t expkey[4 * 15], 16);
        DECLARE_ALIGNED(uint32_t dust[4 * 15], 16);

        printf("AES-CTR standard test vectors:\n");
        for (; v->msg != NULL; v++) {
                struct test_suite_context *ctx;

                if (!quiet_mode) {
#ifdef DEBUG
                        if (alg == IMB_CIPHER_CNTR)
                                printf("Standard vector %zu  KeySize:%zu IVSize:%zu MsgSize:%zu\n",
                                       v->tcId, v->keySize, v->ivSize / 8, v->msgSize / 8);
                        else
                                printf("Bit vector %zu  KeySize:%zu IVSize:%zu MsgSize:%zu\n",
                                       v->tcId, v->keySize, v->ivSize / 8, v->msgSize);
#else
                        printf(".");
#endif
                }

                switch (v->keySize / 8) {
                case IMB_KEY_128_BYTES:
                        IMB_AES_KEYEXP_128(mb_mgr, v->key, expkey, dust);
                        ctx = ctx128;
                        break;
                case IMB_KEY_192_BYTES:
                        IMB_AES_KEYEXP_192(mb_mgr, v->key, expkey, dust);
                        ctx = ctx192;
                        break;
                case IMB_KEY_256_BYTES:
                        IMB_AES_KEYEXP_256(mb_mgr, v->key, expkey, dust);
                        ctx = ctx256;
                        break;
                default:
                        return;
                }

                if (test_ctr(mb_mgr, expkey, (unsigned) v->keySize / 8, v->iv,
                             (unsigned) v->ivSize / 8, (const void *) v->msg, (const void *) v->ct,
                             (unsigned) v->msgSize, IMB_DIR_ENCRYPT, IMB_ORDER_CIPHER_HASH, alg)) {
                        printf("error #%zu encrypt\n", v->tcId);
                        test_suite_update(ctx, 0, 1);
                } else {
                        test_suite_update(ctx, 1, 0);
                }

                if (test_ctr(mb_mgr, expkey, (unsigned) v->keySize / 8, v->iv,
                             (unsigned) v->ivSize / 8, (const void *) v->ct, (const void *) v->msg,
                             (unsigned) v->msgSize, IMB_DIR_DECRYPT, IMB_ORDER_HASH_CIPHER, alg)) {
                        printf("error #%zu decrypt\n", v->tcId);
                        test_suite_update(ctx, 0, 1);
                } else {
                        test_suite_update(ctx, 1, 0);
                }

                if (v->ivSize / 8 == 12 && alg == IMB_CIPHER_CNTR) {
                        /* IV in the table didn't include block counter (12 bytes).
                         * Let's encrypt & decrypt the same but
                         * with 16 byte IV that includes block counter.
                         */
                        const unsigned new_iv_len = 16;
                        const unsigned orig_iv_len = 12;
                        uint8_t local_iv[16];

                        memcpy(local_iv, v->iv, orig_iv_len);
                        /* 32-bit 0x1 in BE == 0x01000000 in LE */
                        local_iv[12] = 0x00;
                        local_iv[13] = 0x00;
                        local_iv[14] = 0x00;
                        local_iv[15] = 0x01;

                        if (test_ctr(mb_mgr, expkey, (unsigned) v->keySize / 8, local_iv,
                                     new_iv_len, (const void *) v->msg, (const void *) v->ct,
                                     (unsigned) v->msgSize, IMB_DIR_ENCRYPT, IMB_ORDER_CIPHER_HASH,
                                     alg)) {
                                printf("error #%zu encrypt\n", v->tcId);
                                test_suite_update(ctx, 0, 1);
                        } else {
                                test_suite_update(ctx, 1, 0);
                        }

                        if (test_ctr(mb_mgr, expkey, (unsigned) v->keySize / 8, local_iv,
                                     new_iv_len, (const void *) v->ct, (const void *) v->msg,
                                     (unsigned) v->msgSize, IMB_DIR_DECRYPT, IMB_ORDER_HASH_CIPHER,
                                     alg)) {
                                printf("error #%zu decrypt\n", v->tcId);
                                test_suite_update(ctx, 0, 1);
                        } else {
                                test_suite_update(ctx, 1, 0);
                        }
                }
        }
        if (!quiet_mode)
                printf("\n");
}

static void
test_ctr_vectors_burst(struct IMB_MGR *mb_mgr, struct test_suite_context *ctx128,
                       struct test_suite_context *ctx192, struct test_suite_context *ctx256,
                       const struct cipher_test *v, const IMB_CIPHER_MODE alg,
                       const uint32_t num_jobs)
{
        DECLARE_ALIGNED(uint32_t expkey[4 * 15], 16);
        DECLARE_ALIGNED(uint32_t dust[4 * 15], 16);

        if (!quiet_mode)
                printf("AES-CTR standard test vectors - Burst API (N jobs = %u):\n", num_jobs);
        for (; v->msg != NULL; v++) {
                struct test_suite_context *ctx;

                if (!quiet_mode) {
#ifdef DEBUG
                        if (alg == IMB_CIPHER_CNTR)
                                printf("Standard vector %zu  KeySize:%zu IVSize:%zu MsgSize:%zu\n",
                                       v->tcId, v->keySize, v->ivSize / 8, v->msgSize / 8);
                        else
                                printf("Bit vector %zu  KeySize:%zu IVSize:%zu MsgSize:%zu\n",
                                       v->tcId, v->keySize, v->ivSize / 8, v->msgSize);
#else
                        printf(".");
#endif
                }

                switch (v->keySize / 8) {
                case IMB_KEY_128_BYTES:
                        IMB_AES_KEYEXP_128(mb_mgr, v->key, expkey, dust);
                        ctx = ctx128;
                        break;
                case IMB_KEY_192_BYTES:
                        IMB_AES_KEYEXP_192(mb_mgr, v->key, expkey, dust);
                        ctx = ctx192;
                        break;
                case IMB_KEY_256_BYTES:
                        IMB_AES_KEYEXP_256(mb_mgr, v->key, expkey, dust);
                        ctx = ctx256;
                        break;
                default:
                        return;
                }

                if (test_ctr_burst(mb_mgr, expkey, (unsigned) v->keySize / 8, v->iv,
                                   (unsigned) v->ivSize / 8, (const void *) v->msg,
                                   (const void *) v->ct, (unsigned) v->msgSize, IMB_DIR_ENCRYPT,
                                   IMB_ORDER_CIPHER_HASH, alg, num_jobs)) {
                        printf("error #%zu encrypt burst\n", v->tcId);
                        test_suite_update(ctx, 0, 1);
                } else {
                        test_suite_update(ctx, 1, 0);
                }

                if (test_ctr_burst(mb_mgr, expkey, (unsigned) v->keySize / 8, v->iv,
                                   (unsigned) v->ivSize / 8, (const void *) v->ct,
                                   (const void *) v->msg, (unsigned) v->msgSize, IMB_DIR_DECRYPT,
                                   IMB_ORDER_HASH_CIPHER, alg, num_jobs)) {
                        printf("error #%zu decrypt burst\n", v->tcId);
                        test_suite_update(ctx, 0, 1);
                } else {
                        test_suite_update(ctx, 1, 0);
                }

                if (v->ivSize / 8 == 12 && alg == IMB_CIPHER_CNTR) {
                        /* IV in the table didn't include block counter (12 bytes).
                         * Let's encrypt & decrypt the same but
                         * with 16 byte IV that includes block counter.
                         */
                        const unsigned new_iv_len = 16;
                        const unsigned orig_iv_len = 12;
                        uint8_t local_iv[16];

                        memcpy(local_iv, v->iv, orig_iv_len);
                        /* 32-bit 0x1 in BE == 0x01000000 in LE */
                        local_iv[12] = 0x00;
                        local_iv[13] = 0x00;
                        local_iv[14] = 0x00;
                        local_iv[15] = 0x01;

                        if (test_ctr_burst(mb_mgr, expkey, (unsigned) v->keySize / 8, local_iv,
                                           new_iv_len, (const void *) v->msg, (const void *) v->ct,
                                           (unsigned) v->msgSize, IMB_DIR_ENCRYPT,
                                           IMB_ORDER_CIPHER_HASH, alg, num_jobs)) {
                                printf("error #%zu encrypt burst\n", v->tcId);
                                test_suite_update(ctx, 0, 1);
                        } else {
                                test_suite_update(ctx, 1, 0);
                        }

                        if (test_ctr_burst(mb_mgr, expkey, (unsigned) v->keySize / 8, local_iv,
                                           new_iv_len, (const void *) v->ct, (const void *) v->msg,
                                           (unsigned) v->msgSize, IMB_DIR_DECRYPT,
                                           IMB_ORDER_HASH_CIPHER, alg, num_jobs)) {
                                printf("error #%zu decrypt burst\n", v->tcId);
                                test_suite_update(ctx, 0, 1);
                        } else {
                                test_suite_update(ctx, 1, 0);
                        }
                }

                /* skip bitlen cipher-only burst api tests */
                if (alg == IMB_CIPHER_CNTR_BITLEN)
                        continue;

                if (test_ctr_cipher_burst(mb_mgr, expkey, (unsigned) v->keySize / 8, v->iv,
                                          (unsigned) v->ivSize / 8, (const void *) v->msg,
                                          (const void *) v->ct, (unsigned) v->msgSize,
                                          IMB_DIR_ENCRYPT, IMB_ORDER_CIPHER_HASH, alg, num_jobs)) {
                        printf("error #%zu encrypt cipher-only burst\n", v->tcId);
                        test_suite_update(ctx, 0, 1);
                } else {
                        test_suite_update(ctx, 1, 0);
                }

                if (test_ctr_cipher_burst(mb_mgr, expkey, (unsigned) v->keySize / 8, v->iv,
                                          (unsigned) v->ivSize / 8, (const void *) v->ct,
                                          (const void *) v->msg, (unsigned) v->msgSize,
                                          IMB_DIR_DECRYPT, IMB_ORDER_HASH_CIPHER, alg, num_jobs)) {
                        printf("error #%zu decrypt cipher-only burst\n", v->tcId);
                        test_suite_update(ctx, 0, 1);
                } else {
                        test_suite_update(ctx, 1, 0);
                }

                if (v->ivSize / 8 == 12 && alg == IMB_CIPHER_CNTR) {
                        /* IV in the table didn't include block counter (12 bytes).
                         * Let's encrypt & decrypt the same but
                         * with 16 byte IV that includes block counter.
                         */
                        const unsigned new_iv_len = 16;
                        const unsigned orig_iv_len = 12;
                        uint8_t local_iv[16];

                        memcpy(local_iv, v->iv, orig_iv_len);
                        /* 32-bit 0x1 in BE == 0x01000000 in LE */
                        local_iv[12] = 0x00;
                        local_iv[13] = 0x00;
                        local_iv[14] = 0x00;
                        local_iv[15] = 0x01;

                        if (test_ctr_cipher_burst(mb_mgr, expkey, (unsigned) v->keySize / 8,
                                                  local_iv, new_iv_len, (const void *) v->msg,
                                                  (const void *) v->ct, (unsigned) v->msgSize / 8,
                                                  IMB_DIR_ENCRYPT, IMB_ORDER_CIPHER_HASH, alg,
                                                  num_jobs)) {
                                printf("error #%zu encrypt cipher-only burst\n", v->tcId);
                                test_suite_update(ctx, 0, 1);
                        } else {
                                test_suite_update(ctx, 1, 0);
                        }

                        if (test_ctr_cipher_burst(mb_mgr, expkey, (unsigned) v->keySize / 8,
                                                  local_iv, new_iv_len, (const void *) v->ct,
                                                  (const void *) v->msg, (unsigned) v->msgSize / 8,
                                                  IMB_DIR_DECRYPT, IMB_ORDER_HASH_CIPHER, alg,
                                                  num_jobs)) {
                                printf("error #%zu decrypt cipher-only burst\n", v->tcId);
                                test_suite_update(ctx, 0, 1);
                        } else {
                                test_suite_update(ctx, 1, 0);
                        }
                }
        }
        if (!quiet_mode)
                printf("\n");
}

int
ctr_test(struct IMB_MGR *mb_mgr)
{
        uint32_t i;
        int errors = 0;
        struct test_suite_context ctx128;
        struct test_suite_context ctx192;
        struct test_suite_context ctx256;

        /* Standard CTR vectors */
        test_suite_start(&ctx128, "AES-CTR-128");
        test_suite_start(&ctx192, "AES-CTR-192");
        test_suite_start(&ctx256, "AES-CTR-256");
        test_ctr_vectors(mb_mgr, &ctx128, &ctx192, &ctx256, ctr_test_json, IMB_CIPHER_CNTR);
        for (i = 1; i <= MAX_CTR_JOBS; i++)
                test_ctr_vectors_burst(mb_mgr, &ctx128, &ctx192, &ctx256, ctr_test_json,
                                       IMB_CIPHER_CNTR, i);
        errors += test_suite_end(&ctx128);
        errors += test_suite_end(&ctx192);
        errors += test_suite_end(&ctx256);

        /* CTR_BITLEN vectors */
        test_suite_start(&ctx128, "AES-CTR-128-BIT-LENGTH");
        test_suite_start(&ctx192, "AES-CTR-192-BIT-LENGTH");
        test_suite_start(&ctx256, "AES-CTR-256-BIT-LENGTH");
        test_ctr_vectors(mb_mgr, &ctx128, &ctx192, &ctx256, ctr_bit_test_json,
                         IMB_CIPHER_CNTR_BITLEN);
        for (i = 1; i <= MAX_CTR_JOBS; i++)
                test_ctr_vectors_burst(mb_mgr, &ctx128, &ctx192, &ctx256, ctr_bit_test_json,
                                       IMB_CIPHER_CNTR_BITLEN, i);
        errors += test_suite_end(&ctx128);
        errors += test_suite_end(&ctx192);
        errors += test_suite_end(&ctx256);

        return errors;
}
