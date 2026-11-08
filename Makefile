# PC-9801 commander. ia16-elf-gcc targets 8086 MS-DOS.
# The Windows build of the compiler lives outside this repo.

IA16 ?= C:/TEMP/ia16/ia16-elf/bin
CC := $(IA16)/ia16-elf-gcc

CFLAGS := -std=gnu89 -Wall -O1 -mcmodel=small -march=i8086 -fno-strict-aliasing
LDFLAGS := -mcmodel=small -Wl,-Map=pc98nc.map

SRCS := main.c vid.c kbd.c dos.c panel.c fileop.c view.c util.c crit.s

.PHONY: all clean
all: pc98nc.exe

pc98nc.exe: $(SRCS) pc98.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRCS) -li86

clean:
	rm -f pc98nc.exe pc98nc.map
