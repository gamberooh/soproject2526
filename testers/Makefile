
XT_PRG_PREFIX = riscv64-unknown-elf-
CC = $(XT_PRG_PREFIX)gcc
LD = $(XT_PRG_PREFIX)ld
AS = $(XT_PRG_PREFIX)as
OBJCOPY = $(XT_PRG_PREFIX)objcopy
OBJDUMP = $(XT_PRG_PREFIX)objdump 


URISCV_DIR_PREFIX = /usr/local
URISCV_DATA_DIR = $(URISCV_DIR_PREFIX)/share/uriscv
URISCV_INCLUDE_DIR = /usr/include
URISCV_LIB_DIR = /usr/lib/uriscv

TDEFS = h/print.h h/tconst.h $(URISCV_INCLUDE_DIR)/uriscv/liburiscv.h Makefile

CFLAGS_LANG = -ffreestanding -static -c -nostdlib
CFLAGS = $(CFLAGS_LANG) -I$(URISCV_INCLUDE_DIR) -Wall -O0 -march=rv32imfd -mabi=ilp32d


LDAOUTFLAGS = -G 0 -nostdlib -T $(URISCV_DATA_DIR)/uriscvaout.ldscript -march=rv32imfd -melf32lriscv
LDCOREFLAGS = -G 0 -nostdlib -T $(URISCV_DATA_DIR)/uriscvcore.ldscript


EF = uriscv-elf2uriscv
UDEV = uriscv-mkdev


all: shell.uriscv fibEight.uriscv echo.uriscv fibEleven.uriscv uname.uriscv calc.uriscv date.uriscv sl.uriscv

%.o: %.c $(TDEFS)
	$(CC) $(CFLAGS) $<

%.t: %.o print.o $(URISCV_LIB_DIR)/crti.o
	$(LD) $(LDAOUTFLAGS) $(URISCV_LIB_DIR)/crti.o $< print.o $(URISCV_LIB_DIR)/liburiscv.o -o $@

%.t.aout.uriscv: %.t
	$(EF) -a $<

%.uriscv: %.t.aout.uriscv
	$(UDEV) -f $@ $<

clean:
	rm -f *.o *.t *.uriscv
	