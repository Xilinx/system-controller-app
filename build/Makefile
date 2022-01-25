#
# Copyright (c) 2020 - 2022 Xilinx, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

APP		= sc_app
APPD		= sc_appd
DEPS		= $(APP).h

BOARD_OBJS	= sc_vck190.o sc_vpk120.o
BIT_OBJS	= sc_BIT.o
COMMON_OBJS	= sc_common.o sc_parse.o
APP_OBJS	= $(APP).o
APPD_OBJS	= $(APPD).o $(COMMON_OBJS) $(BOARD_OBJS) $(BIT_OBJS)

CFLAGS		= -I../src -Wall
LDFLAGS 	?= -L../src
SRCDIR		= ../src

all: $(APP) $(APPD)

%.o: $(SRCDIR)/%.c $(SRCDIR)/$(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP): $(APP_OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lm -lgpiod

$(APPD): $(APPD_OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lm -lrt -lgpiod

clean:
	rm -f $(APP) $(APPD) *.o
