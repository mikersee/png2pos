; yasm -f elf64 X.asm
; ld -s -o X X.o

section	.data
	msg db 0x1d, 0x50, 0xb4, 0xb4 ; GS P 180 180
	    db 0x1d, 0x56, 0x41, 0x63 ; GS V full 99 points
	    db 0x1b, 0x4a, 0x08 ; ESC J 8 points
	len equ $ - msg

section .text
	global _start

_start:
	mov eax, 4
	mov ebx, 1
	mov ecx, msg
	mov edx, len
	int 80h

	mov eax, 1
	mov ebx, 0
	int 80h
