BITS 64

loop:
    movzx bx, BYTE[rax]
    movzx cx, BYTE [rax+1]
    add bx, cx
    mov WORD [rax+2], bx
    jmp loop
    ret

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
