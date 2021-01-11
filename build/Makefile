#
# Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

PLAT	:= vck190
BIT	:= BIT
APP	= sc_app
APPD	= sc_appd
COMMON	= sc_common
CFLAGS	= -I../src
LDFLAGS ?= -L../src
SRCDIR	= ../src
APP_OBJS	= $(APP).o $(COMMON).o sc_$(PLAT).o sc_$(BIT).o
APPD_OBJS	= $(APPD).o $(COMMON).o sc_$(PLAT).o
DEPS	= $(APP).h

all: $(APP) $(APPD)

%.o: $(SRCDIR)/%.c $(SRCDIR)/$(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP): $(APP_OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lm

$(APPD): $(APPD_OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lm -lrt -lgpiod

clean:
	rm -f $(APP) $(APPD) *.o
