; 0BSD, 2023, Fanda Uchytil
;
; Build: nasm -O0 -f bin mbr.nasm -o mbr.bin
;
BITS    16                      ; MBR operates in 16 bit (real) mode
ORG     0x7c00                  ; Origin address -> where the BIOS places the program in memory
;org     0x0100                  ; DOS loading address (for testing in dosbox)


; RIFF magic header
db "RIFF"                       ; Dissasembles to:
                                ;   52      push dx
                                ;   49      dec cx
                                ;   46      inc si
                                ;   46      inc si
; RIFF size
;db 0xe2                         ; `LOOP` instruction (has side effect: cx--) [size = 9954]
db 0x90                         ; NOP
db 0xe9                         ; short jump instruction [size = 2550160 = 2.4 MiB]
db (_start - $ - 1)             ; 1 byte (jump) offset
db 0x00                         ; "padding" -> RIFF size is 4 bytes, "E9" jump has size of 3 bytes.

; WebP magic header (another RIFF container)
db "WEBP"

; VP8X -- extended header (will be filled by the webp_completor)
db "VP8X"                       ; VP8X chunk id (0x58385056 ; "56503858")
dd 0x0000000a                   ; VP8X chunk size (always 10 bytes)
times 0xa db 0x00               ; Reserve the space

; Custom Header
;db "EXIF"                       ; EXIF chunk id (0x46495845 ; "45584946")
db "HACK"                       ; HACK chunk id (0x4b434148 ; "4841434b")
dd _code_size                   ; Chunk size of our code

_payload:

_start:

    mov ax, 0x0002              ; Set video mode (text mode 80x25)
    int 0x10                    ; https://en.wikipedia.org/wiki/INT_10H

    mov ax, 0xb800              ; Video memory; https://wiki.osdev.org/Printing_to_Screen
    mov es, ax

    xor di, di
    mov cx, 80 * 25
    mov ax, 0b0_001_1001__0010_0000 ; Color (bg:blue, fg:blue) + character (space) ; https://en.wikipedia.org/wiki/BIOS_color_attributes
    rep stosw                   ; Fill the screen (writes 'ax' => 2 bytes)

    mov ah,0b0_000_1001         ; fg color = blue

    ; draw lines
    mov al, 0xcd                ; line char
    mov di, ((80 * 11) + 0) * 2 ; x = 0, y = 11
    mov cl, 80                  ; num rows
    rep stosw
    mov di, ((80 * 19) + 0) * 2 ; x = 0, y = 19
    mov cl, 80                  ; num rows
    rep stosw

    ; draw backgrounds
    mov al, 0xb0                ; the most transtapernt tile
    mov di, ((80 * 6) + 0) * 2  ; x = 0, y = 6
    mov cx, 80 * 5              ; num rows * lines
    rep stosw

    mov al, 0xb1                ; more transparent tile
    mov di, ((80 * 12) + 0) * 2 ; x = 0, y = 19
    mov cx, 80 * 7              ; num rows + lines
    rep stosw


    push 0xb0dc                 ; bg = the most transtapernt tile, fg = bottom half tile
    mov di, ((80 * 7) + 0) * 2  ; x = 0, y = 7
    mov bx, htp0
    mov cx, htp0_size
    call print_bin_pic
    ;add sp, 2

    push 0xb1dd                 ; bg = more transparent tile, fg = left half tile
    mov di, ((80 * 13) + 0) * 2 ; x = 0, y = 13
    mov bx, htp1
    mov cx, htp1_size
    call print_bin_pic
    ;add sp, 2

    ; print ascii art

    mov ah, 0b0_010_1010

    push ((80 * 1) + 2) * 2     ; x = 2, y = 1
    mov bx, htp2
    mov cx, htp2_size
    call print_text

    push ((80 * 21) + 2) * 2    ; x = 2, y = 21
    mov bx, htp3
    mov cx, htp3_size
    call print_text

    mov ah, 0b0_011_1010

    push ((80 * 24) + 80 - h4x_cz_size) * 2 ; x = 80 - h4x_cz_size, y = 24
    mov bx, h4x_cz
    mov cx, h4x_cz_size
    call print_text


    ; forever loop
    jmp $


print_text:
    mov bp, sp
    mov di, [bp + 2]
    .loop:
        mov al, [cs:bx]
        cmp al, 0x0a
        jne .print_ansi
            add word [bp + 2], 80*2
            mov di, [bp + 2]
            ;add di, 80*2
            jmp .skip_printing
        .print_ansi:
            stosw
        .skip_printing:
        inc bx
        loop .loop
    ret

print_bin_pic:
    mov bp, sp
    .byte_loop:
        mov dl, [cs:bx]
        mov si, 8                       ; byte width

        .bit_loop:
            test dl, 0b10000000
            mov al, byte [bp + 2]       ; character
            jnz .skip_space
                mov al, byte [bp + 3]   ; space
            .skip_space:

            stosw

            shl dl, 1
            dec si
            test si, si
            jnz .bit_loop

        inc bx
        loop .byte_loop
    ret



;;
;; Galxy brain compression
;;
;;  - bit array: '#' = 1, ' ' = 0
;;

; # #  ##   ## # #   ### # # ###   ### #    ##  #  # ### ### 
; ### #  # #   ##     #  ### ##    ### #   #  # ## # ##   #  
; # # #  #  ## # #    #  # # ###   #   ### #  # # ## ###  #  
htp0:
    db 0b00000000,0b01010011,0b00011010,0b10001110,0b10101110,0b00111010,0b00011001,0b00101110,0b11100010,0b00000000
    db 0b00000000,0b01110100,0b10100011,0b00000100,0b11101100,0b00111010,0b00100101,0b10101100,0b01000010,0b00000000
    db 0b00000000,0b01010100,0b10011010,0b10000100,0b10101110,0b00100011,0b10100101,0b01101110,0b01000010,0b00000000
htp0_size equ $-htp0

; #  #  ##   ##  #  #    ##### #  # ####    ###  #     ##  #   # #### #####   ## 
; #  # #  # #  # # #       #   #  # #       #  # #    #  # ##  # #      #     ## 
; #### #### #    ##        #   #### ###     ###  #    #### # # # ###    #     ## 
; #  # #  # #  # # #       #   #  # #       #    #    #  # #  ## #      #        
; #  # #  #  ##  #  #      #   #  # ####    #    #### #  # #   # ####   #     ## 
htp1:
    db 0b01001001,0b10001100,0b10010000,0b11111010,0b01011110,0b00011100,0b10000011,0b00100010,0b11110111,0b11000110,
    db 0b01001010,0b01010010,0b10100000,0b00100010,0b01010000,0b00010010,0b10000100,0b10110010,0b10000001,0b00000110,
    db 0b01111011,0b11010000,0b11000000,0b00100011,0b11011100,0b00011100,0b10000111,0b10101010,0b11100001,0b00000110,
    db 0b01001010,0b01010010,0b10100000,0b00100010,0b01010000,0b00010000,0b10000100,0b10100110,0b10000001,0b00000000,
    db 0b01001010,0b01001100,0b10010000,0b00100010,0b01011110,0b00010000,0b11110100,0b10100010,0b11110001,0b00000110,
htp1_size equ $-htp1

htp2:
    db "They're trashing our rights, man!", 0x0a
    db "They're trashing the flow of data!", 0x0a
    db "Trashing!", 0x0a
    db "Hack the planet!", 0xa
htp2_size equ $-htp2

htp3:
    db "Remember, hacking is more than just a crime.", 0x0a
    db "It's a survival trait."
htp3_size equ $-htp3

h4x_cz:
    db 0x10, " With ", 0x03, ", h4x.cz ", 0x11
h4x_cz_size equ $-h4x_cz



times 510 - ($ - $$) db 0x4f    ; Fill the space up to 510 bytes
db 0x55, 0xaa                   ; Mark it as MBR

_code_size equ $ - _payload
