; boot2.asm
[map symbols documentation/boot2.map]
[Bits 16]
org 0x500
jmp entry_point

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Memory Management:
; org                   500
; data/extra segment      0
; stack               9FC00
; BL1 including BPB    7C00
; PM kernel          100000
; memory tables        1000
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;*******************************************************
;    Includes and Defines
;*******************************************************
%include "gdt.inc"                ; GDT definition
%include "A20.inc"                ; A20 gate enabling
%include "Fat12.inc"              ; FAT12 driver
%include "GetMemoryMap.inc"       ; INT 0x15, eax = 0xE820

%define IMAGE_PMODE_BASE 0x100000 ; final kernel location in Protected Mode


;*******************************************************
;    Data Section
;*******************************************************
ImageName         db "KERNEL  BIN"
msgBootLoaderName db "PrettyBL", 0
msgGTD            db 0x0D, 0x0A, 0x0D, 0x0A, "GTD installed", 0
msgUnrealMode     db 0x0D, 0x0A, "Unreal Mode entered", 0
msgLoadKernel     db 0x0D, 0x0A, "Loading Kernel...", 0
msgFloppyMotorOff db 0x0D, 0x0A, "Floppy Disk Motor switched off", 0
msgSwitchToPM     db 0x0D, 0x0A, "Switching to Protected Mode (PM)...", 0
msgFailure        db 0x0D, 0x0A, "KERNEL.BIN missing (Fatal Error)", 0


[BITS 16]
entry_point:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ax, 0x9000       ; stack
    mov ss, ax
    mov sp, 0xFC00       ; stacksegment:stackpointer (linear): 9FC00h (below BIOS area)

A20:
    call EnableA20

;*******************************************************
;    Determine physical memory INT 0x15, eax = 0xE820
;    input: es:di (destination buffer)
;*******************************************************

Get_Memory_Map:
    mov di, 0x1000
    call get_memory_by_int15_e820

Install_GDT:
    call InstallGDT
    mov si, msgGTD
    call print_string

    call EnterUnrealMode
    mov si, msgUnrealMode
    call print_string

Load_Root:
    mov si, msgLoadKernel
    call print_string

    call LoadRoot
    mov edi, IMAGE_PMODE_BASE
    mov si, ImageName

    call LoadFile       ; c.f. FAT12.inc

    test ax, ax
    je EnterProtectedMode
    mov si, msgFailure
    call print_string

;*******************************************************
;    Switch from Real Mode (RM) to Protected Mode (PM)
;*******************************************************
EnterProtectedMode:
    ; switch off floppy disk motor
    mov dx,0x3F2
    mov al,0x0C
    out dx,al
    mov si, msgFloppyMotorOff
    call print_string

    ; switch to PM
    mov si, msgSwitchToPM
    call print_string
    cli
    mov eax, cr0                       ; bit 0 in CR0 has to be set for entering PM
    or al, 1
    mov cr0, eax
    jmp DWORD CODE_DESC:ProtectedMode  ; far jump to code selector (cs = 0x08)

[Bits 32]
ProtectedMode:
    mov ax, DATA_DESC                  ; set data segments to data selector (0x10)
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov esp, 0x9000

PrepareMultiboot:
    mov ebx, 0x1000-0x58               ; 0x58 == sizeof(multiboot_t)
    mov [ebx + 0x00], DWORD 0b1001000001
    mov [ebx + 0x04], DWORD 640
    call convert_mmap
    mov [ebx + 0x2C], eax              ; Store size of mmap
    mov eax, [0x1200]
    shr eax, 10
    mov [ebx + 0x08], eax
    mov [ebx + 0x30], WORD 0x1100
    mov [ebx + 0x40], DWORD msgBootLoaderName
    mov eax, 0x2BADB002                ; Magic number
    cli                                ; Multiboot requires IF to be cleared

;*******************************************************
;    Execute Kernel
;*******************************************************
EXECUTE:
    jmp DWORD CODE_DESC:IMAGE_PMODE_BASE + 0x0C ; Jump behind multiboot header. TODO: Use other header size if flags are set
    cli
    hlt

;*******************************************************
;    calls, e.g. print_string
;*******************************************************
[BITS 16]
print_string:
    mov ah, 0x0E                    ; BIOS function 0x0E: teletype
    .loop:
        lodsb                       ; grab a byte from SI
        test al, al                 ; NULL?
        jz .done                    ; if zero: end loop
        int 0x10                    ; else: print character to screen
        jmp .loop
    .done:
        ret
