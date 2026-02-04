global start
extern long_mode_start

section .text
bits 32
start:
    mov esp, stack_top
    
    ; Save multiboot info pointer (ebx contains it from GRUB)
    mov [multiboot_info_ptr], ebx

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call set_up_page_tables
    call enable_paging

    ; Load the 64-bit GDT
    lgdt [gdt64.pointer]

    jmp gdt64.code_segment:long_mode_start

check_multiboot:
    cmp eax, 0x36d76289
    jne .no_multiboot
    ret
.no_multiboot:
    mov al, "M"
    jmp error

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, "C"
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, "L"
    jmp error

set_up_page_tables:
    ; Map first P4 entry to P3 table
    mov eax, p3_table
    or eax, 0b11 ; present + writable
    mov [p4_table], eax
    mov dword [p4_table + 4], 0

    ; Map first four P3 entries to P2 tables (4 * 1GB = 4GB)
    mov eax, p2_table
    or eax, 0b11 ; present + writable
    mov [p3_table], eax
    mov dword [p3_table + 4], 0

    mov eax, p2_table + 4096
    or eax, 0b11 ; present + writable
    mov [p3_table + 8], eax
    mov dword [p3_table + 12], 0

    mov eax, p2_table + 8192
    or eax, 0b11 ; present + writable
    mov [p3_table + 16], eax
    mov dword [p3_table + 20], 0

    mov eax, p2_table + 12288
    or eax, 0b11 ; present + writable
    mov [p3_table + 24], eax
    mov dword [p3_table + 28], 0

    ; Map each P2 entry to a huge 2MB page (4 * 512 * 2MB = 4GB)
    mov ecx, 0         ; counter
.map_p2_table:
    ; Map ecx-th P2 entry to a huge page that starts at 2MB * ecx
    mov eax, 0x200000  ; 2MB
    mul ecx            ; start address of ecx-th page
    or eax, 0b10000011 ; present + writable + huge
    mov [p2_table + ecx * 8], eax ; map lower 32 bits
    mov dword [p2_table + ecx * 8 + 4], 0 ; zero upper 32 bits

    inc ecx            ; increase counter
    cmp ecx, 512 * 4   ; if counter == 2048, 4GB are mapped
    jne .map_p2_table  ; else map the next entry

    ret

enable_paging:
    ; Load P4 to cr3
    mov eax, p4_table
    mov cr3, eax

    ; Enable PAE-flag in cr4 (Physical Address Extension)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Set the long mode bit in the EFER MSR (Model Specific Register)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging in the cr0 register
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

error:
    ; Print "ERR: X" where X is the error code in al
    mov dword [0xb8000], 0x4f524f45
    mov dword [0xb8004], 0x4f3a4f52
    mov dword [0xb8008], 0x4f204f20
    mov byte  [0xb800a], al
    hlt

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096 * 4
stack_bottom:
    resb 4096 * 4
stack_top:

section .rodata
gdt64:
    dq 0 ; zero entry
.code_segment: equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; code segment
.data_segment: equ $ - gdt64
    dq (1 << 44) | (1 << 47) | (1 << 41) ; data segment
.pointer:
    dw $ - gdt64 - 1
    dq gdt64 ; 64-bit address


section .data
global multiboot_info_ptr
multiboot_info_ptr:
    dq 0

section .text
bits 64
extern kmain

long_mode_start:
    ; load 0 into all data segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Ensure the stack pointer is properly initialized for 64-bit mode
    mov rsp, stack_top

    ; Debug: Print 'K' to VGA text buffer to show we reached long mode
    mov rax, 0x2f4b2f202f532f4f ; "O S   K "
    mov qword [0xb8000], rax

    ; Pass multiboot info pointer to kmain (in rdi)
    ; ebx was saved before entering long mode, need to get it
    ; Actually, ebx contains multiboot info in 32-bit mode
    ; We need to save it before the jump
    ; For now, let's use a fixed approach
    
    call kmain
.hang:
    hlt
    jmp .hang

