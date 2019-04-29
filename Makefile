# Makefile
# BASICloader (under development)
# by Richard Cavell (c) 2017-2019

CC         += -std=c89
CFLAGS     += -Wall -Werror -Wextra -Wconversion -Wpedantic -fmax-errors=3

SOURCE      = BASICloader.c
EXECUTABLE  = BASICloader

.DEFAULT: all
.PHONY:   all
all:      BASICloader

$(EXECUTABLE): $(SOURCE)
	$(CC) $(CFLAGS) $< $(OUTPUT_OPTION)

.PHONY: clean
clean:
	$(RM) $(EXECUTABLE)
