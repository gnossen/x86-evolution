BITS 64

    movzx bx, BYTE[rax]
    movzx cx, BYTE [rax+1]
    add bx, cx
    mov WORD [rax+2], bx
    ret

    ; Ideal Solution
    ; movzx bx, BYTE[rax]
    ; movzx cx, BYTE [rax+1]
    ; add bx, cx
    ; mov rcx, rax
    ; mov ax, 10
    ; mul bx
    ; mov WORD [rcx+2], ax
    ; ret
