	.file	"example_instrumented.bc"
	.text
	.globl	DYTAN_tag
	.align	16, 0x90
	.type	DYTAN_tag,@function
DYTAN_tag:                              # @DYTAN_tag
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
	.size	DYTAN_tag, .Ltmp0-DYTAN_tag

	.globl	DYTAN_display
	.align	16, 0x90
	.type	DYTAN_display,@function
DYTAN_display:                          # @DYTAN_display
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
	.size	DYTAN_display, .Ltmp1-DYTAN_display

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
	pushl	%edi
	pushl	%esi
	subl	$32, %esp
	movl	8(%ebp), %edi
	movl	%edi, -12(%ebp)
	movl	%edi, (%esp)
	calll	malloc
	movl	%eax, %esi
	movl	%edi, 4(%esp)
	movl	%esi, (%esp)
	movl	$5, 8(%esp)
	calll	DYTAN_tag
	movl	%esi, -24(%ebp)
	movl	-12(%ebp), %edi
	movl	%esi, (%esp)
	calll	DYTAN_free
	movl	%edi, 4(%esp)
	movl	%esi, (%esp)
	calll	realloc
	movl	%eax, %esi
	movl	%edi, 4(%esp)
	movl	%esi, (%esp)
	movl	$5, 8(%esp)
	calll	DYTAN_tag
	testl	%esi, %esi
	movl	%esi, -24(%ebp)
	je	.LBB3_5
# BB#1:                                 # %if.end
	movl	count, %eax
	movl	%eax, -20(%ebp)
	movl	%eax, (%esp)
	calll	srand
	movl	-24(%ebp), %eax
	movl	-12(%ebp), %ecx
	movl	%ecx, 4(%esp)
	movl	%eax, (%esp)
	movl	$.L.str, 8(%esp)
	calll	DYTAN_display
	movl	$0, -16(%ebp)
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
	movl	-16(%ebp), %ecx
	movl	-24(%ebp), %edx
	movb	%al, (%edx,%ecx)
	incl	-16(%ebp)
.LBB3_2:                                # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	movl	-16(%ebp), %eax
	cmpl	-12(%ebp), %eax
	jle	.LBB3_3
# BB#4:                                 # %for.end
	movl	-12(%ebp), %eax
	movl	-24(%ebp), %ecx
	movb	$0, -1(%eax,%ecx)
	movl	-24(%ebp), %eax
	movl	-12(%ebp), %ecx
	movl	%ecx, 4(%esp)
	movl	%eax, (%esp)
	movl	$.L.str1, 8(%esp)
	calll	DYTAN_display
	movl	-24(%ebp), %esi
	movl	%esi, (%esp)
	calll	free
	movl	%esi, (%esp)
	calll	DYTAN_free
	movl	-24(%ebp), %eax
	movl	-12(%ebp), %ecx
	movl	%ecx, 4(%esp)
	movl	%eax, (%esp)
	movl	$.L.str2, 8(%esp)
	calll	DYTAN_display
	movl	-24(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	$.L.str3, (%esp)
	calll	printf
.LBB3_5:                                # %return
	addl	$32, %esp
	popl	%esi
	popl	%edi
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

	.type	count,@object           # @count
	.data
	.globl	count
	.align	4
count:
	.long	7                       # 0x7
	.size	count, 4

	.type	.L.str,@object          # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"buffer before for"
	.size	.L.str, 18

	.type	.L.str1,@object         # @.str1
.L.str1:
	.asciz	"buffer before free"
	.size	.L.str1, 19

	.type	.L.str2,@object         # @.str2
.L.str2:
	.asciz	"buffer after DYTAN free"
	.size	.L.str2, 24

	.type	.L.str3,@object         # @.str3
.L.str3:
	.asciz	"Random string: %s\n"
	.size	.L.str3, 19


	.ident	"clang version 3.4 (trunk 193096) (llvm/trunk 193095)"
	.section	".note.GNU-stack","",@progbits
