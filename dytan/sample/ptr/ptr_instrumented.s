	.file	"ptr_instrumented.bc"
	.text
	.globl	DYTAN_tag_pointer
	.align	16, 0x90
	.type	DYTAN_tag_pointer,@function
DYTAN_tag_pointer:                      # @DYTAN_tag_pointer
# BB#0:                                 # %entry
	pushl	%ebp
	movl	%esp, %ebp
	subl	$12, %esp
	movl	8(%ebp), %eax
	movl	%eax, -4(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -8(%ebp)
	movl	16(%ebp), %eax
	movl	%eax, -12(%ebp)
	addl	$12, %esp
	popl	%ebp
	ret
.Ltmp0:
	.size	DYTAN_tag_pointer, .Ltmp0-DYTAN_tag_pointer

	.globl	DYTAN_tag_memory
	.align	16, 0x90
	.type	DYTAN_tag_memory,@function
DYTAN_tag_memory:                       # @DYTAN_tag_memory
# BB#0:                                 # %entry
	pushl	%ebp
	movl	%esp, %ebp
	subl	$12, %esp
	movl	16(%ebp), %eax
	movl	12(%ebp), %ecx
	movl	8(%ebp), %edx
	movl	%edx, -4(%ebp)
	movl	%ecx, -8(%ebp)
	movl	%eax, -12(%ebp)
	addl	$12, %esp
	popl	%ebp
	ret
.Ltmp1:
	.size	DYTAN_tag_memory, .Ltmp1-DYTAN_tag_memory

	.globl	DYTAN_free
	.align	16, 0x90
	.type	DYTAN_free,@function
DYTAN_free:                             # @DYTAN_free
# BB#0:                                 # %entry
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%eax
	movl	8(%ebp), %eax
	movl	%eax, -4(%ebp)
	addl	$4, %esp
	popl	%ebp
	ret
.Ltmp2:
	.size	DYTAN_free, .Ltmp2-DYTAN_free

	.globl	prRandStr
	.align	16, 0x90
	.type	prRandStr,@function
prRandStr:                              # @prRandStr
# BB#0:                                 # %entry
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	subl	$44, %esp
	movl	8(%ebp), %ebx
	movl	%ebx, -16(%ebp)
	movl	%ebx, (%esp)
	calll	malloc
	movl	%eax, %esi
	movl	%esi, -28(%ebp)
	movl	ima_tag_count, %edi
	movl	%edi, 8(%esp)
	leal	-28(%ebp), %eax
	movl	%eax, (%esp)
	movl	$4, 4(%esp)
	calll	DYTAN_tag_pointer
	movl	%edi, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	%esi, (%esp)
	calll	DYTAN_tag_memory
	incl	%edi
	movl	%edi, ima_tag_count
	movl	-28(%ebp), %edi
	movl	-16(%ebp), %ebx
	addl	%ebx, %ebx
	movl	%ebx, 4(%esp)
	movl	%edi, (%esp)
	calll	realloc
	movl	%eax, -36(%ebp)         # 4-byte Spill
	movl	%eax, -28(%ebp)
	movl	%edi, (%esp)
	calll	DYTAN_free
	movl	ima_tag_count, %edi
	movl	%edi, 8(%esp)
	leal	-28(%ebp), %esi
	movl	%esi, (%esp)
	movl	$4, 4(%esp)
	calll	DYTAN_tag_pointer
	movl	%edi, 8(%esp)
	movl	%ebx, 4(%esp)
	movl	-36(%ebp), %eax         # 4-byte Reload
	movl	%eax, (%esp)
	calll	DYTAN_tag_memory
	incl	%edi
	movl	%edi, ima_tag_count
	movl	%esi, %edi
	movl	%edi, 4(%esp)
	movl	$.L.str, (%esp)
	calll	printf
	movl	-28(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	$.L.str1, (%esp)
	calll	printf
	cmpl	$0, -28(%ebp)
	je	.LBB3_5
# BB#1:                                 # %if.end
	movl	seed_value, %eax
	movl	%eax, -24(%ebp)
	movl	%eax, (%esp)
	calll	srand
	movb	$0, -29(%ebp)
	movl	$0, -20(%ebp)
	movl	$1321528399, %esi       # imm = 0x4EC4EC4F
	jmp	.LBB3_2
	.align	16, 0x90
.LBB3_3:                                # %for.inc
                                        #   in Loop: Header=BB3_2 Depth=1
	calll	rand
	movl	%eax, %ecx
	imull	%esi
	movl	%edx, %eax
	shrl	$31, %eax
	shrl	$3, %edx
	addl	%eax, %edx
	imull	$26, %edx, %eax
	negl	%eax
	leal	97(%ecx,%eax), %eax
	movl	-20(%ebp), %ecx
	movl	-28(%ebp), %edx
	movb	%al, (%edx,%ecx)
	incl	-20(%ebp)
.LBB3_2:                                # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	movl	-20(%ebp), %eax
	movl	-16(%ebp), %ecx
	addl	%ecx, %ecx
	cmpl	%ecx, %eax
	jle	.LBB3_3
# BB#4:                                 # %for.end
	movl	-16(%ebp), %eax
	addl	%eax, %eax
	movl	-28(%ebp), %ecx
	movb	$0, -1(%ecx,%eax)
	movl	-28(%ebp), %esi
	movl	%esi, (%esp)
	calll	free
	movl	%esi, (%esp)
	calll	DYTAN_free
	movl	-28(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	$.L.str2, (%esp)
	calll	printf
	movl	%edi, 4(%esp)
	movl	$.L.str, (%esp)
	calll	printf
	movl	-28(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	$.L.str1, (%esp)
	calll	printf
.LBB3_5:                                # %return
	addl	$44, %esp
	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp
	ret
.Ltmp3:
	.size	prRandStr, .Ltmp3-prRandStr

	.globl	main
	.align	16, 0x90
	.type	main,@function
main:                                   # @main
# BB#0:                                 # %entry
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
	movl	12(%ebp), %eax
	movl	8(%ebp), %ecx
	movl	%ecx, -4(%ebp)
	movl	%eax, -8(%ebp)
	movl	4(%eax), %eax
	movl	%eax, (%esp)
	calll	atoi
	movl	%eax, (%esp)
	calll	prRandStr
	xorl	%eax, %eax
	addl	$24, %esp
	popl	%ebp
	ret
.Ltmp4:
	.size	main, .Ltmp4-main

	.type	seed_value,@object      # @seed_value
	.data
	.globl	seed_value
	.align	4
seed_value:
	.long	7                       # 0x7
	.size	seed_value, 4

	.type	.L.str,@object          # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"pointer %p\n"
	.size	.L.str, 12

	.type	.L.str1,@object         # @.str1
.L.str1:
	.asciz	"memory %p\n"
	.size	.L.str1, 11

	.type	.L.str2,@object         # @.str2
.L.str2:
	.asciz	"Random string: %s\n"
	.size	.L.str2, 19

	.type	ima_tag_count,@object   # @ima_tag_count
	.bss
	.globl	ima_tag_count
	.align	4
ima_tag_count:
	.long	0                       # 0x0
	.size	ima_tag_count, 4


	.ident	"clang version 3.4 (trunk 193096) (llvm/trunk 193095)"
	.section	".note.GNU-stack","",@progbits
