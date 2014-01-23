	.code16
	.section ".init","ax"
	.globl _start
_start:
	jmp main
	.type _start,@function
	.size _start,.-_start
