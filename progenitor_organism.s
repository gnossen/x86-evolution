BITS 64

loop:
    movzx bx, BYTE[rax]
    nop
    movzx cx, BYTE [rax+1]
    nop
    mov rdx, 0x0
    nop
    mov dx, WORD [rdx] ; Generates sigsegv.
    nop
    add bx, cx
    nop
    mov WORD [rax+2], bx
    nop
    jmp loop


    ; Ideal Solution
    ; movzx bx, BYTE[rax]
    ; movzx cx, BYTE [rax+1]
    ; add bx, cx
    ; mov rcx, rax
    ; mov ax, 10
    ; mul bx
    ; mov WORD [rcx+2], ax
    ; mov rax, rcx
    ; jmp loop
    ; ret
