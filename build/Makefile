#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

APP		= sc_app
APPD		= sc_appd
DEPS		= $(APP).h

BIT_OBJS	= sc_BIT.o
OTHER_OBJS	= sc_common.o sc_parse.o sc_board.o
APP_OBJS	= $(APP).o
APPD_OBJS	= $(APPD).o $(OTHER_OBJS) $(BIT_OBJS)

GIT_COMMIT	= "$(shell git describe --abbrev=40 --always)"
GIT_BRANCH	= "$(shell git rev-parse --abbrev-ref HEAD)"

CFLAGS		= -I../src -Wall -DGIT_COMMIT=\"$(GIT_COMMIT)\" -DGIT_BRANCH=\"$(GIT_BRANCH)\"
LDFLAGS 	?= -L../src
SRCDIR		= ../src

all: $(APP) $(APPD)

%.o: $(SRCDIR)/%.c $(SRCDIR)/$(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP): $(APP_OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(APPD): $(APPD_OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lm -lrt -lgpiod

clean:
	rm -f $(APP) $(APPD) *.o
