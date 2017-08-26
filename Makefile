# BASICloader (under development)
# by Richard Cavell (c) 2017
# Makefile

CC += -std=c89
CFLAGS += -Wall -Werror -Wextra -Wconversion -Wpedantic -fmax-errors=3

EXECUTABLE = BASICloader

.DEFAULT: all
.PHONY: all
all: BASICloader

$(EXECUTABLE): $(EXECUTABLE).c
	$(CC) $(CFLAGS) $< $(OUTPUT_OPTION)

.PHONY: clean
clean:
	$(RM) $(EXECUTABLE)
