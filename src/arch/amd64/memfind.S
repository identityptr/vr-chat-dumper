        .text
        .p2align 4
        .globl memfind
        .type memfind, @function

memfind:
        .cfi_startproc
        pushq   %rbx
        .cfi_adjust_cfa_offset 8
        .cfi_offset %rbx, -16
        pushq   %r12
        .cfi_adjust_cfa_offset 8
        .cfi_offset %r12, -24
        pushq   %r13
        .cfi_adjust_cfa_offset 8
        .cfi_offset %r13, -32
        pushq   %r14
        .cfi_adjust_cfa_offset 8
        .cfi_offset %r14, -40
        pushq   %r15
        .cfi_adjust_cfa_offset 8
        .cfi_offset %r15, -48

        movq    %rdi, %r12            /* immutable data base */
        movq    %rdx, %r13            /* immutable pattern base */
        movq    %rcx, %r14            /* immutable pattern length */

        testq   %r14, %r14
        jz      .Lempty_pattern
        cmpq    %r14, %rsi
        jb      .Lnot_found
        cmpq    $1, %r14
        je      .Lsingle_byte

        movq    %rsi, %r15
        subq    %r14, %r15
        xorl    %ebx, %ebx            /* current sixteen-byte window */

        movzbl  (%r13), %eax
        movd    %eax, %xmm0
        punpcklbw %xmm0, %xmm0
        punpcklwd %xmm0, %xmm0
        pshufd  $0, %xmm0, %xmm0

        .p2align 4
.Lvector_window:
        movq    %r15, %rax
        subq    %rbx, %rax
        cmpq    $15, %rax
        jb      .Lscalar_tail

        movdqu  (%r12,%rbx), %xmm1
        pcmpeqb %xmm0, %xmm1
        pmovmskb %xmm1, %r9d          /* one candidate bit per lane */

.Lnext_vector_candidate:
        testl   %r9d, %r9d
        jz      .Ladvance_window

        bsfl    %r9d, %ecx
        btrl    %ecx, %r9d
        leaq    (%rbx,%rcx), %r8
        jmp     .Lverify_candidate

.Lcandidate_failed:
        jmp     .Lnext_vector_candidate

.Ladvance_window:
        addq    $16, %rbx
        jmp     .Lvector_window

.Lscalar_tail:
        cmpq    %r15, %rbx
        ja      .Lnot_found

        .p2align 4
.Lscalar_candidate:
        movzbl  (%r13), %eax
        cmpb    %al, (%r12,%rbx)
        jne     .Ladvance_scalar
        movq    %rbx, %r8

.Lverify_candidate:
        leaq    (%r12,%r8), %rdi
        leaq    -1(%r14), %rax
        movzbl  (%r13,%rax), %edx
        cmpb    %dl, (%rdi,%rax)
        jne     .Lverification_failed

        cmpq    $2, %r14
        je      .Lfound

        /* Compare bytes [1, pattern_size - 1). */
        incq    %rdi
        leaq    1(%r13), %rsi
        leaq    -2(%r14), %rcx
        repe cmpsb
        je      .Lfound

.Lverification_failed:
        cmpq    %r15, %rbx
        ja      .Lnot_found
        movq    %r15, %rax
        subq    %rbx, %rax
        cmpq    $15, %rax
        jae     .Lcandidate_failed

.Ladvance_scalar:
        incq    %rbx
        cmpq    %r15, %rbx
        jbe     .Lscalar_candidate
        jmp     .Lnot_found

.Lsingle_byte:
        movq    %rsi, %rcx
        movq    %r12, %rdi
        movzbl  (%r13), %eax
        repne scasb
        jne     .Lnot_found
        subq    %r12, %rdi
        leaq    -1(%rdi), %rax
        jmp     .Lreturn

.Lfound:
        movq    %r8, %rax
        jmp     .Lreturn

.Lempty_pattern:
        xorl    %eax, %eax
        jmp     .Lreturn

.Lnot_found:
        movq    $-1, %rax

.Lreturn:
        popq    %r15
        .cfi_restore %r15
        .cfi_adjust_cfa_offset -8
        popq    %r14
        .cfi_restore %r14
        .cfi_adjust_cfa_offset -8
        popq    %r13
        .cfi_restore %r13
        .cfi_adjust_cfa_offset -8
        popq    %r12
        .cfi_restore %r12
        .cfi_adjust_cfa_offset -8
        popq    %rbx
        .cfi_restore %rbx
        .cfi_adjust_cfa_offset -8
        ret
        .cfi_endproc

        .size memfind, .-memfind
        .section .note.GNU-stack,"",@progbits
