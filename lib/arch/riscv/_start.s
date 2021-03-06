.text
.align 4

.equ NR_exit, 93

.global _start
.global _exit

_start:
	li	x8, 0			/* clear the frame pointer */
	la	x3, _gp			/* init global pointer */
	ld	a0, 0(sp)		/* argc */
	addi	a1, sp, 8		/* argv */

	slli	a2, a0, 3		/* 8*argc */
	add	a2, a2, a1		/* 8*argc + argv */
	addi	a2, a2, 8		/* 8*argc + argv + 8 = envp */

	jal	main

_exit:
	li	a7, NR_exit
	ecall

.type _start,function
.size _start,_exit-_start

.type _exit,function
.size _exit,.-_exit
