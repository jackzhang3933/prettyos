# Define OS-dependant Tools
ifeq ($(OS),WINDOWS)
    RM= - del
    MV= cmd /c move /Y
    NASM= nasm
    CC= i586-elf-gcc
    LD= i586-elf-ld
else
    RM= rm -f
    MV= mv
    NASM= nasm
    CC= gcc
    LD= ld
endif

# Folders
OBJDIR= object_files
STAGE1DIR= stage1_bootloader
STAGE2DIR= stage2_bootloader
KERNELDIR= kernel
USERDIR= user
SHELLDIR= user_program_c
USERRDDIR= init_rd_img
USERTEST= user_test_c
USERTOOLS= user_tools

# dependancies
KERNEL_OBJECTS := $(patsubst %.c, %.o, $(wildcard $(KERNELDIR)/*.c $(KERNELDIR)/cdi/*.c)) $(patsubst %.asm, %.o, $(wildcard $(KERNELDIR)/*.asm))
SHELL_OBJECTS := $(patsubst %.c, %.o, $(wildcard $(USERDIR)/$(USERTOOLS)/*.c $(USERDIR)/$(SHELLDIR)/*.c)) $(patsubst %.asm, %.o, $(wildcard $(USERDIR)/$(USERTOOLS)/*.asm))
TESTC_OBJCETS := $(patsubst %.c, %.o, $(wildcard $(USERDIR)/$(USERTOOLS)/*.c $(USERDIR)/$(USERTEST)/*.c)) $(patsubst %.asm, %.o, $(wildcard $(USERDIR)/$(USERTOOLS)/*.asm))

# Compiler-/Linker-Flags
NASMFLAGS= -O32 -f elf
GCCFLAGS= -c -m32 -std=c99 -Wshadow -march=i386 -mtune=i386 -m32 -fno-pic -Werror -Wall -O -ffreestanding -fleading-underscore -nostdinc -fno-builtin -fno-stack-protector -Iinclude
LDFLAGS= -nostdlib

# targets to build one asm or c-file to an object file
vpath %.o $(OBJDIR)
%.o: %.c 
	$(CC) $< $(GCCFLAGS) -I $(KERNELDIR)/include -I $(USERDIR)/$(USERTOOLS) -o $(OBJDIR)/$@
%.o: %.asm
	$(NASM) $< $(NASMFLAGS) -I$(KERNELDIR)/ -o $(OBJDIR)/$@

# targets to build PrettyOS
.PHONY: clean all
all: $(STAGE1DIR)/boot.bin $(STAGE2DIR)/BOOT2.BIN $(KERNELDIR)/KERNEL.BIN FloppyImage.bin

$(STAGE1DIR)/boot.bin:
	$(NASM) -f bin $(STAGE1DIR)/boot.asm -I$(STAGE1DIR)/ -o $(STAGE1DIR)/boot.bin
$(STAGE2DIR)/BOOT2.BIN:
	$(NASM) -f bin $(STAGE2DIR)/boot2.asm -I$(STAGE2DIR)/ -o $(STAGE2DIR)/BOOT2.BIN

$(KERNELDIR)/KERNEL.BIN: $(KERNELDIR)/initrd.dat $(KERNEL_OBJECTS)
	$(LD) $(LDFLAGS) $(addprefix $(OBJDIR)/,$(KERNEL_OBJECTS)) -T $(KERNELDIR)/kernel.ld -Map $(KERNELDIR)/kernel.map -o $(KERNELDIR)/KERNEL.BIN

$(USERDIR)/$(SHELLDIR)/program.elf: $(SHELL_OBJECTS)
	$(LD) $(LDFLAGS) $(addprefix $(OBJDIR)/,$(SHELL_OBJECTS)) -T $(USERDIR)/$(USERTOOLS)/user.ld -Map $(USERDIR)/$(SHELLDIR)/shell.map -o $(USERDIR)/$(SHELLDIR)/program.elf

$(KERNELDIR)/initrd.dat: $(USERDIR)/$(SHELLDIR)/program.elf
	tools/make_initrd $(USERDIR)/$(USERRDDIR)/info.txt info $(USERDIR)/$(SHELLDIR)/program.elf shell
	$(MV) initrd.dat $(KERNELDIR)/initrd.dat

$(USERDIR)/$(USERTEST)/HELLO.ELF: $(TESTC_OBJCETS)
	$(LD) $(LDFLAGS) $(addprefix $(OBJDIR)/,$(TESTC_OBJECTS)) -T $(USERDIR)/$(USERTOOLS)/user.ld -Map $(USERDIR)/$(USERTEST)/user.map -o $(USERDIR)/$(SHELLDIR)/HELLO.ELF
	$(LD) $(LDFLAGS) $(addprefix $(OBJDIR)/,$(TESTC_OBJECTS)) -T $(USERDIR)/$(USERTOOLS)/user.ld -Map $(USERDIR)/$(USERTEST)/user.map -o $(USERDIR)/$(USERTEST)/HELLO.ELF

FloppyImage.bin: $(STAGE1DIR)/boot.bin $(STAGE2DIR)/BOOT2.BIN $(KERNELDIR)/KERNEL.BIN #$(USERDIR)/$(USERTEST)/HELLO.ELF
	tools/CreateFloppyImage2 PrettyOS FloppyImage.bin $(STAGE1DIR)/boot.bin $(STAGE2DIR)/BOOT2.BIN $(KERNELDIR)/KERNEL.BIN $(USERDIR)/$(USERTEST)/HELLO.ELF

clean:
# OS-dependant code because of different interpretation of / in Windows and Linux
ifeq ($(OS),WINDOWS)
	$(RM) $(OBJDIR)\$(KERNELDIR)\*.o
	$(RM) $(OBJDIR)\$(KERNELDIR)\cdi\*.o
	$(RM) $(OBJDIR)\$(USERDIR)\$(USERTOOLS)\*.o
	$(RM) $(OBJDIR)\$(USERDIR)\$(SHELLDIR)\*.o
	$(RM) $(OBJDIR)\$(USERDIR)\$(USERTEST)\*.o
else
	$(RM) $(OBJDIR)/$(KERNELDIR)/*.o
	$(RM) $(OBJDIR)/$(KERNELDIR)/cdi/*.o
	$(RM) $(OBJDIR)/$(USERDIR)/$(USERTOOLS)/*.o
	$(RM) $(OBJDIR)/$(USERDIR)/$(SHELLDIR)/*.o
	$(RM) $(OBJDIR)/$(USERDIR)/$(USERTEST)/*.o
endif