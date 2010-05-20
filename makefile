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
    ifeq ($(OS),MACOSX)
        CC= i586-elf-gcc
        LD= i586-elf-ld
    else
        CC= gcc
        LD= ld
    endif
endif

# Folders
OBJDIR= object_files
STAGE1DIR= stage1_bootloader
STAGE2DIR= stage2_bootloader
KERNELDIR= kernel
USERDIR= user
SHELLDIR= shell
USERRDDIR= init_rd_img
USERTESTC= user_test_c
USERTESTCPP= user_test_cpp
USERTOOLS= user_tools

# dependancies
KERNEL_OBJECTS := $(patsubst %.c, %.o, $(wildcard $(KERNELDIR)/*.c $(KERNELDIR)/cdi/*.c)) $(patsubst %.asm, %.o, $(wildcard $(KERNELDIR)/*.asm))
SHELL_OBJECTS := $(patsubst %.c, %.o, $(wildcard $(USERDIR)/$(USERTOOLS)/*.c $(USERDIR)/$(SHELLDIR)/*.c)) $(patsubst %.asm, %.o, $(wildcard $(USERDIR)/$(USERTOOLS)/*.asm))

# Compiler-/Linker-Flags
NASMFLAGS= -O32 -f elf
GCCFLAGS= -c -std=c99 -march=i386 -Wshadow -mtune=i386 -m32 -Werror -Wall -s -Os -ffreestanding -fleading-underscore -nostdinc -fno-pic -fno-builtin -fno-stack-protector -fno-common -Iinclude
LDFLAGS= -nostdlib --warn-common

# targets to build one asm or c-file to an object file
vpath %.o $(OBJDIR)
%.o: %.c 
	$(CC) $< $(GCCFLAGS) -I $(KERNELDIR)/include -I $(USERDIR)/$(USERTOOLS) -o $(OBJDIR)/$@
%.o: %.asm
	$(NASM) $< $(NASMFLAGS) -I$(KERNELDIR)/ -o $(OBJDIR)/$@

# targets to build PrettyOS
.PHONY: clean all

all: FloppyImage.img

$(STAGE1DIR)/boot.bin:
	$(NASM) -f bin $(STAGE1DIR)/boot.asm -I$(STAGE1DIR)/ -o $(STAGE1DIR)/boot.bin
$(STAGE2DIR)/BOOT2.BIN:
	$(NASM) -f bin $(STAGE2DIR)/boot2.asm -I$(STAGE2DIR)/ -o $(STAGE2DIR)/BOOT2.BIN

$(KERNELDIR)/KERNEL.BIN: $(KERNELDIR)/initrd.dat $(KERNEL_OBJECTS)
#	because changes in the Shell should change data.o we build data.o everytimes
	$(NASM) $(KERNELDIR)/data.asm $(NASMFLAGS) -I$(KERNELDIR)/ -o $(OBJDIR)/$(KERNELDIR)/data.o
	$(LD) $(LDFLAGS) $(addprefix $(OBJDIR)/,$(KERNEL_OBJECTS)) -T $(KERNELDIR)/kernel.ld -Map $(KERNELDIR)/kernel.map -o $(KERNELDIR)/KERNEL.BIN

$(USERDIR)/$(SHELLDIR)/shell.elf: $(SHELL_OBJECTS)
	$(LD) $(LDFLAGS) $(addprefix $(OBJDIR)/,$(SHELL_OBJECTS)) -nmagic -T $(USERDIR)/$(USERTOOLS)/user.ld -Map $(USERDIR)/$(SHELLDIR)/shell.map -o $(USERDIR)/$(SHELLDIR)/shell.elf

$(KERNELDIR)/initrd.dat: $(USERDIR)/$(SHELLDIR)/shell.elf
	tools/make_initrd $(USERDIR)/$(USERRDDIR)/info.txt info $(USERDIR)/$(SHELLDIR)/shell.elf shell
	$(MV) initrd.dat $(KERNELDIR)/initrd.dat

FloppyImage.img: $(STAGE1DIR)/boot.bin $(STAGE2DIR)/BOOT2.BIN $(KERNELDIR)/KERNEL.BIN
	tools/CreateFloppyImage2 PRETTYOS FloppyImage.img $(STAGE1DIR)/boot.bin $(STAGE2DIR)/BOOT2.BIN $(KERNELDIR)/KERNEL.BIN $(USERDIR)/$(USERTESTC)/HELLO.ELF $(USERDIR)/$(USERTESTCPP)/CPP.ELF $(USERDIR)/other_userprogs/CALC.ELF $(USERDIR)/other_userprogs/MUSIC.ELF $(USERDIR)/other_userprogs/README.ELF $(USERDIR)/other_userprogs/TTT.ELF $(USERDIR)/other_userprogs/PQEQ.ELF

clean:
# OS-dependant code because of different interpretation of "/" in Windows and UNIX-OS (Linux and Mac OS X)
ifeq ($(OS),WINDOWS)
	$(RM) $(STAGE1DIR)\boot.bin
	$(RM) $(STAGE2DIR)\BOOT2.BIN
	$(RM) $(OBJDIR)\$(KERNELDIR)\*.o
	$(RM) $(KERNELDIR)\KERNEL.BIN
	$(RM) $(OBJDIR)\$(KERNELDIR)\cdi\*.o
	$(RM) $(OBJDIR)\$(USERDIR)\$(USERTOOLS)\*.o
	$(RM) $(OBJDIR)\$(USERDIR)\$(SHELLDIR)\*.o
	$(RM) $(USERDIR)\$(SHELLDIR)\shell.elf
else
	$(RM) $(STAGE1DIR)/boot.bin
	$(RM) $(STAGE2DIR)/BOOT2.BIN
	$(RM) $(OBJDIR)/$(KERNELDIR)/*.o
	$(RM) $(KERNELDIR)/KERNEL.BIN
	$(RM) $(OBJDIR)/$(KERNELDIR)/cdi/*.o
	$(RM) $(OBJDIR)/$(USERDIR)/$(USERTOOLS)/*.o
	$(RM) $(OBJDIR)/$(USERDIR)/$(SHELLDIR)/*.o
	$(RM) $(USERDIR)/$(SHELLDIR)/shell.elf
endif