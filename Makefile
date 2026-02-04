# SPDX-License-Identifier: GPL-2.0-or-later
CC ?= gcc

CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Werror
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -Iinclude -Isrc

LDFLAGS ?=

BIN := mpsutil
SRC := \
	src/mptutil.c \
	src/mpt_mpi.c \
	src/show.c \
	src/util.c

.PHONY: all clean

all: $(BIN) mprutil

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

# mprutil behaves identically; argv[0] selects /dev/mpt3ctl vs /dev/mpt2ctl.
mprutil: $(BIN)
	ln -sf $(BIN) $@

clean:
	rm -f $(BIN) mprutil
