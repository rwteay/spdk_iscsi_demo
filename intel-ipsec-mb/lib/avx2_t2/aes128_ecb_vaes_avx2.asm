;;
;; Copyright (c) 2022-2023, Intel Corporation
;;
;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are met:
;;
;;     * Redistributions of source code must retain the above copyright notice,
;;       this list of conditions and the following disclaimer.
;;     * Redistributions in binary form must reproduce the above copyright
;;       notice, this list of conditions and the following disclaimer in the
;;       documentation and/or other materials provided with the distribution.
;;     * Neither the name of Intel Corporation nor the names of its contributors
;;       may be used to endorse or promote products derived from this software
;;       without specific prior written permission.
;;
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
;; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;; SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;; CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;; OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;

; routine to do AES ECB encrypt/decrypt on 16n bytes doing AES by 16

; YMM registers are clobbered. Saving/restoring must be done at a higher level

; void aes_ecb_x_y_vaes_avx2(void    *in,
;                      UINT128  keys[],
;                      void    *out,
;                      UINT64   len_bytes);
;
; x = direction (enc/dec)
; y = key size (128/192/256)
; arg 1: IN:   pointer to input (cipher text)
; arg 2: KEYS: pointer to keys
; arg 3: OUT:  pointer to output (plain text)
; arg 4: LEN:  length in bytes (multiple of 16)
;

%include "include/os.inc"
%include "include/clear_regs.inc"
%include "include/aes_common.inc"

%ifdef LINUX
%define IN      rdi
%define KEYS    rsi
%define OUT     rdx
%define LEN     rcx
%else
%define IN      rcx
%define KEYS    rdx
%define OUT     r8
%define LEN     r9
%endif
%define IDX     rax
%define TMP     r11

%define YKEY1       ymm1
%define YDATA0      ymm2
%define YDATA1      ymm3
%define YDATA2      ymm4
%define YDATA3      ymm5
%define YDATA4      ymm6
%define YDATA5      ymm7
%define YDATA6      ymm8
%define YDATA7      ymm9

%ifndef AES_ECB_NROUNDS
%define AES_ECB_NROUNDS 10
%endif

%if AES_ECB_NROUNDS == 10
%define KEYSIZE 128
%elif AES_ECB_NROUNDS == 12
%define KEYSIZE 192
%else
%define KEYSIZE 256
%endif

%define AES_ECB_ENC aes_ecb_enc_ %+ KEYSIZE %+ _vaes_avx2
%define AES_ECB_DEC aes_ecb_dec_ %+ KEYSIZE %+ _vaes_avx2

%macro AES_ECB 1
%define %%DIR     %1 ; [in] Direction (ENC/DIR)
%ifidn %%DIR, ENC
%define AES      YMM_AESENC_ROUND_BLOCKS_AVX2_0_16
%else ; DIR = DEC
%define AES      YMM_AESDEC_ROUND_BLOCKS_AVX2_0_16
%endif

        or      LEN, LEN
        jz      %%done

        xor     IDX, IDX
        mov     TMP, LEN
        and     TMP, 255    ; number of initial bytes (0 to 15 AES blocks)
        jz      %%main_loop

        ; branch to different code block based on remainder
        cmp     TMP, 8*16
        je      %%initial_num_blocks_is_8
        jb      %%initial_num_blocks_is_7_1
        cmp     TMP, 12*16
        je      %%initial_num_blocks_is_12
        jb      %%initial_num_blocks_is_11_9
        ;; 15, 14 or 13
        cmp     TMP, 14*16
        ja      %%initial_num_blocks_is_15
        je      %%initial_num_blocks_is_14
        jmp     %%initial_num_blocks_is_13
%%initial_num_blocks_is_11_9:
        ;; 11, 10 or 9
        cmp     TMP, 10*16
        ja      %%initial_num_blocks_is_11
        je      %%initial_num_blocks_is_10
        jmp     %%initial_num_blocks_is_9
%%initial_num_blocks_is_7_1:
        cmp     TMP, 4*16
        je      %%initial_num_blocks_is_4
        jb      %%initial_num_blocks_is_3_1
        ;; 7, 6 or 5
        cmp     TMP, 6*16
        ja      %%initial_num_blocks_is_7
        je      %%initial_num_blocks_is_6
        jmp     %%initial_num_blocks_is_5
%%initial_num_blocks_is_3_1:
        ;; 3, 2 or 1
        cmp     TMP, 2*16
        ja      %%initial_num_blocks_is_3
        je      %%initial_num_blocks_is_2
        ;; fall through for `jmp %%initial_num_blocks_is_1`

%assign num_blocks 1
%rep 15

        %%initial_num_blocks_is_ %+ num_blocks :
%assign %%I 0
        ; load initial blocks
        YMM_LOAD_BLOCKS_AVX2_0_16 num_blocks, IN, 0, YDATA0,\
                YDATA1, YDATA2, YDATA3, YDATA4, YDATA5,\
                YDATA6, YDATA7

; Perform AES encryption/decryption on initial blocks
%rep (AES_ECB_NROUNDS + 1)          ; 10/12/14
        vbroadcasti128      YKEY1, [KEYS + %%I*16]
        AES YDATA0, YDATA1, YDATA2, YDATA3, YDATA4,\
                YDATA5, YDATA6, YDATA7, YKEY1, %%I, no_data,\
                no_data, no_data, no_data, no_data, no_data,\
                no_data, no_data, num_blocks, (AES_ECB_NROUNDS - 1)
%assign %%I (%%I + 1)
%endrep

        ; store initial blocks
        YMM_STORE_BLOCKS_AVX2_0_16 num_blocks, OUT, 0, YDATA0, YDATA1,\
                YDATA2, YDATA3, YDATA4, YDATA5, YDATA6, YDATA7

        add     IDX, num_blocks*16
        cmp     IDX, LEN
        je      %%done

%assign num_blocks (num_blocks + 1)
        jmp     %%main_loop
%endrep

align 16
%%main_loop:
        ; load the next 16 blocks into ymm registers
        YMM_LOAD_BLOCKS_AVX2_0_16 16, {IN + IDX}, 0, YDATA0, YDATA1,\
                YDATA2, YDATA3, YDATA4, YDATA5, YDATA6, YDATA7

        ; Perform AES encryption/decryption on 16 blocks
%assign %%ROUNDNO 0        ; current key number
%rep (AES_ECB_NROUNDS + 1)          ; 10/12/14
        vbroadcasti128      YKEY1, [KEYS + %%ROUNDNO*16]

        AES YDATA0, YDATA1, YDATA2, YDATA3, YDATA4, YDATA5,\
                YDATA6, YDATA7, YKEY1, %%ROUNDNO, no_data, no_data,\
                no_data, no_data, no_data, no_data, no_data, no_data,\
                16, (AES_ECB_NROUNDS - 1)

%assign %%ROUNDNO (%%ROUNDNO + 1)
%endrep

        ; store 16 blocks
        YMM_STORE_BLOCKS_AVX2_0_16 16, {OUT + IDX}, 0, YDATA0, YDATA1,\
                YDATA2, YDATA3, YDATA4, YDATA5, YDATA6, YDATA7

        add     IDX, 16*16
        cmp     IDX, LEN
        jne     %%main_loop

%%done:

%ifdef SAFE_DATA
        clear_all_ymms_asm
%else
        vzeroupper
%endif
%endmacro

mksection .text
align 16
MKGLOBAL(AES_ECB_ENC,function,internal)
AES_ECB_ENC:
        AES_ECB ENC
        ret
align 16
MKGLOBAL(AES_ECB_DEC,function,internal)
AES_ECB_DEC:
        AES_ECB DEC
        ret

mksection stack-noexec
