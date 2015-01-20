; yasm -f elf64 X.asm
; ld -s -o X X.o

section	.data
	msg db 0x1b, 0x40 ; ESC @
	    db 0x1b, 0x52, 0x00 ; ESC R, USA
	    db 0x1b, 0x74, 0x12 ; ESC t, PC852
	    db 0x1b, 0x4d, 0x01 ; ESC m, Font B
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
