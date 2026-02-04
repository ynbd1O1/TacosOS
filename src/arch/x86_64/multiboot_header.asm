section .multiboot_header
align 8
header_start:
    dd 0xe85250d6                ; Magic number (multiboot 2)
    dd 0                         ; Architecture 0 (protected mode i386)
    dd header_end - header_start ; Header length
    ; Checksum
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; End tag
    align 8
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:
