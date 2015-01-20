; yasm -f elf64 X.asm
; ld -s -o X X.o

section	.data
	msg db 0x1b, 0x40 ; ESC @
	    db 0x1d, 0x28, 0x41, 0x02, 0x00, 0x00, 0x02 ; GS ( A 02 00 00 02
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
