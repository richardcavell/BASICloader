# BASICloader by Richard Cavell
# Copyright (c) 2025
# Makefile

.DEFAULT: all

BLDRNAME  = basicldr

# Compilation flags
CC        =  gcc
CFLAGS    = -std=c89 -Wpedantic
CFLAGS   += -Wall -Wextra -Werror -fmax-errors=1
CFLAGS   += -Walloca -Wbad-function-cast -Wcast-align -Wcast-qual -Wconversion
CFLAGS   += -Wdisabled-optimization -Wdouble-promotion -Wduplicated-cond
CFLAGS   += -Werror=format-security -Werror=implicit-function-declaration
CFLAGS   += -Wfloat-equal -Wformat=2 -Wformat-overflow -Wformat-truncation
CFLAGS   += -Wlogical-op -Wmissing-prototypes -Wmissing-declarations
CFLAGS   += -Wno-missing-field-initializers -Wnull-dereference
CFLAGS   += -Woverlength-strings -Wpointer-arith -Wredundant-decls -Wshadow
CFLAGS   += -Wsign-conversion -Wstack-protector -Wstrict-aliasing
CFLAGS   += -Wstrict-overflow -Wswitch-default -Wswitch-enum
CFLAGS   += -Wundef -Wunreachable-code -Wunsafe-loop-optimizations
CFLAGS   += -fstack-protector-strong
CFLAGS   += -g -O2
LDFLAGS   = -Wl,-z,defs -Wl,-O1 -Wl,--gc-sections -Wl,-z,relro

.PHONY: all clean help info license test-coco
all: $(BLDRNAME)

$(BLDRNAME): BASICloader.c
	@echo "Building" $(BLDRNAME) "..."
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $< -o $(BLDRNAME)
	@echo "Finished building" $(BLDRNAME)

clean:
	@rm -f -v $(BLDRNAME)

help info:
	@echo "Your options are:"
	@echo "make all"
	@echo "make clean"
	@echo "make info"
	@echo "make license"
	@echo "make test-coco"

license:
	@cat LICENSE

# This assumes that you have installed asm6809 and XRoar, both by Ciaran Anscomb
test-coco: $(BLDRNAME)
	@echo "Building testcoco.bin..."
	@asm6809 -o Tests/testcoco.bin Tests/testcoco.asm
	@echo "Done"
	@echo "Now running BASICloader..."
	@./$(BLDRNAME) --target coco --begin 4000 -o Tests/testcoco.bas --uppercase Tests/testcoco.bin
	@echo "Done"
	@echo "Now running XRoar..."
	@xroar -machine coco -run Tests/testcoco.bas
	@echo "Done"
