[Bits 32]

; http://www.lowlevel.eu/wiki/Multiboot
GRUB_FLAGS           equ 10b                               ; Flags for GRUB header
GRUB_MAGIC_NUMBER    equ 0x1BADB002                        ; Magic number for GRUB header
GRUB_HEADER_CHECKSUM equ -(GRUB_MAGIC_NUMBER + GRUB_FLAGS) ; Checksum for GRUB header

align 4
MultiBootHeader:            ; This is the "multiboot" header for GRUB
    dd GRUB_MAGIC_NUMBER
    dd GRUB_FLAGS
    dd GRUB_HEADER_CHECKSUM


KernelStart:
    mov esp, 0x1000000

    push ebx     ; EBX points to the multiboot structure created by the bootloader and containing e.g. the address of the memory map

    extern main ; entry point in ckernel.c
    call   main

    cli
    hlt
