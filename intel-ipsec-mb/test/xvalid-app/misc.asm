;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Copyright(c) 2019-2023, Intel Corporation All rights reserved.
;
;  Redistribution and use in source and binary forms, with or without
;  modification, are permitted provided that the following conditions
;  are met:
;    * Redistributions of source code must retain the above copyright
;      notice, this list of conditions and the following disclaimer.
;    * Redistributions in binary form must reproduce the above copyright
;      notice, this list of conditions and the following disclaimer in
;      the documentation and/or other materials provided with the
;      distribution.
;    * Neither the name of Intel Corporation nor the names of its
;      contributors may be used to endorse or promote products derived
;      from this software without specific prior written permission.
;
;  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


%ifdef LINUX
;;; macro to declare global symbols
;;;  - name : symbol name
;;;  - type : function or data
;;;  - scope : internal, private, default
%define MKGLOBAL(name,type,scope) global name %+ : %+ type scope

;;; ABI function arguments
%define arg1    rdi
%define arg2    rsi
%define arg3    rdx
%define arg4    rcx

%define arg1d   edi
%define arg2d   esi
%define arg3d   edx
%define arg4d   ecx
%endif

%ifdef WIN_ABI
;;; macro to declare global symbols
;;;  - name : symbol name
;;;  - type : function or data
;;;  - scope : internal, private, default (ignored in win64 coff format)
%define MKGLOBAL(name,type,scope) global name

;;; ABI function arguments
%define arg1    rcx
%define arg2    rdx
%define arg3    r8
%define arg4    r9

%define arg1d   ecx
%define arg2d   edx
%define arg3d   r8d
%define arg4d   r9d
%endif

section .bss
default rel

MKGLOBAL(gps,data,)
align 8
gps:	        resq	14

MKGLOBAL(simd_regs,data,)
alignb 64
simd_regs:	resb	32*64

section .text

;; uint32_t avx_sse_transition_check(void)
MKGLOBAL(avx_sse_transition_check,function,)
align 16
avx_sse_transition_check:
        mov     ecx, 1
        xgetbv
        ;; result goes to edx:eax
        ;; we care about bits 2 and 6 only
        and     eax, (1 << 2) | (1 << 6)
        ret

;; void *nosimd_memcpy(void *dst, const void *src, size_t n)
MKGLOBAL(nosimd_memcpy,function,)
align 16
nosimd_memcpy:
        pushfq
        push    arg1
        cld                     ;; increment dst/src pointers

%ifdef WIN_ABI
        push    rdi
        push    rsi
        mov     rdi, arg1       ;; arg1 = rcx
        mov     rsi, arg2       ;; arg2 = rdx
        mov     rcx, arg3       ;; arg3 = r8
        rep movsb
        pop     rsi
        pop     rdi
%endif

%ifdef LINUX
        ;; rdi = arg1
        ;; rsi = arg2
        mov     rcx, arg3       ;; arg3 = rdx
        rep movsb
%endif

        pop     rax             ;; return `dst`
        popfq
        ret

;; void *nosimd_memset(void *p, int c, size_t n)
MKGLOBAL(nosimd_memset,function,)
align 16
nosimd_memset:
        pushfq
        push    arg1
        cld                     ;; increment dst pointer

%ifdef WIN_ABI
        push    rdi
        mov     rdi, arg1       ;; arg1 = rcx
        mov     rax, arg2       ;; arg2 = rdx
        mov     rcx, arg3       ;; arg3 = r8
        rep stosb
        pop     rdi
%endif

%ifdef LINUX
        ;; rdi = arg1
        mov     rax, arg2       ;; arg2 = rsi
        mov     rcx, arg3       ;; arg3 = rdx
        rep stosb
%endif

        pop     rax             ;; return `p`
        popfq
        ret

;; Returns RSP pointer with the value BEFORE the call, so 8 bytes need
;; to be added
MKGLOBAL(rdrsp,function,)
align 16
rdrsp:
        lea rax, [rsp + 8]
        ret

MKGLOBAL(dump_gps,function,)
align 16
dump_gps:

        mov     [rel gps],      rax
        mov     [rel gps + 8],  rbx
        mov     [rel gps + 16], rcx
        mov     [rel gps + 24], rdx
        mov     [rel gps + 32], rdi
        mov     [rel gps + 40], rsi

%assign i 8
%assign j 0
%rep 8
        mov     [rel gps + 48 + j], r %+i
%assign i (i+1)
%assign j (j+8)
%endrep

        ret

MKGLOBAL(dump_xmms_sse,function,)
align 16
dump_xmms_sse:

%assign i 0
%assign j 0
%rep 16
        movdqa  [rel simd_regs + j], xmm %+i
%assign i (i+1)
%assign j (j+16)
%endrep

        ret

MKGLOBAL(dump_xmms_avx,function,)
align 16
dump_xmms_avx:

%assign i 0
%assign j 0
%rep 16
        vmovdqa [rel simd_regs + j], xmm %+i
%assign i (i+1)
%assign j (j+16)
%endrep

        ret

MKGLOBAL(dump_ymms,function,)
align 16
dump_ymms:

%assign i 0
%assign j 0
%rep 16
        vmovdqa [rel simd_regs + j], ymm %+i
%assign i (i+1)
%assign j (j+32)
%endrep

        ret

MKGLOBAL(dump_zmms,function,)
align 16
dump_zmms:

%assign i 0
%assign j 0
%rep 32
        vmovdqa64 [rel simd_regs + j], zmm %+i
%assign i (i+1)
%assign j (j+64)
%endrep

        ret

;
; This function clears all scratch XMM registers
;
; void clr_scratch_xmms_sse(void)
MKGLOBAL(clr_scratch_xmms_sse,function,internal)
align 16
clr_scratch_xmms_sse:

%ifdef LINUX
%assign i 0
%rep 16
        pxor    xmm %+ i, xmm %+ i
%assign i (i+1)
%endrep
; On Windows, XMM0-XMM5 registers are scratch registers
%else
%assign i 0
%rep 6
        pxor    xmm %+ i, xmm %+ i
%assign i (i+1)
%endrep
%endif ; LINUX

        ret

;
; This function clears all scratch XMM registers
;
; It should be called before restoring the XMM registers
; for Windows (XMM6-XMM15)
;
; void clr_scratch_xmms_avx(void)
MKGLOBAL(clr_scratch_xmms_avx,function,internal)
align 16
clr_scratch_xmms_avx:

%ifdef LINUX
        vzeroall
; On Windows, XMM0-XMM5 registers are scratch registers
%else
%assign i 0
%rep 6
        vpxor   xmm %+ i, xmm %+ i
%assign i (i+1)
%endrep
%endif ; LINUX

        ret

;
; This function clears all scratch YMM registers
;
; It should be called before restoring the XMM registers
; for Windows (XMM6-XMM15)
;
; void clr_scratch_ymms(void)
MKGLOBAL(clr_scratch_ymms,function,internal)
align 16
clr_scratch_ymms:
; On Linux, all YMM registers are scratch registers
%ifdef LINUX
        vzeroall
; On Windows, YMM0-YMM5 registers are scratch registers.
; YMM6-YMM15 upper 128 bits are scratch registers too, but
; the lower 128 bits are to be restored after calling these function
; which clears the upper bits too.
%else
%assign i 0
%rep 6
        vpxor   ymm %+ i, ymm %+ i
%assign i (i+1)
%endrep
%endif ; LINUX

        ret

;
; This function clears all scratch ZMM registers
;
; It should be called before restoring the XMM registers
; for Windows (XMM6-XMM15). YMM registers are used
; on purpose, since XOR'ing YMM registers is faster
; than XOR'ing ZMM registers, and the operation clears
; also the upper 256 bits
;
; void clr_scratch_zmms(void)
MKGLOBAL(clr_scratch_zmms,function,internal)
align 16
clr_scratch_zmms:

; On Linux, all ZMM registers are scratch registers
%ifdef LINUX
        vzeroall
        ;; vzeroall only clears the first 16 ZMM registers
%assign i 16
%rep 16
        vpxorq  ymm %+ i, ymm %+ i
%assign i (i+1)
%endrep
; On Windows, ZMM0-ZMM5 and ZMM16-ZMM31 registers are scratch registers.
; ZMM6-ZMM15 upper 384 bits are scratch registers too, but
; the lower 128 bits are to be restored after calling these function
; which clears the upper bits too.
%else
%assign i 0
%rep 6
        vpxorq  ymm %+ i, ymm %+ i
%assign i (i+1)
%endrep

%assign i 16
%rep 16
        vpxorq  ymm %+ i, ymm %+ i
%assign i (i+1)
%endrep
%endif ; LINUX

        ret

;;
;; Wrapper for CPUID opcode
;;
;; Parameters:
;;    [in] leaf    - CPUID leaf number (EAX)
;;    [in] subleaf - CPUID sub-leaf number (ECX)
;;    [out] out    - registers structure to store results of CPUID into
;;
;; void misc_cpuid(const unsigned leaf, const unsigned subleaf, struct cpuid_regs *out)

MKGLOBAL(misc_cpuid,function,internal)
misc_cpuid:
        push    rbx

        mov     r11, arg3       ;; arg3 will get overwritten by cpuid on sysv
        mov     eax, arg1d
        mov     ecx, arg2d

        cpuid

        mov     [r11 + 0*4], eax
        mov     [r11 + 1*4], ebx
        mov     [r11 + 2*4], ecx
        mov     [r11 + 3*4], edx

        pop     rbx
        ret

%ifdef LINUX
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
