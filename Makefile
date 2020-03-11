CC	:= ${CROSS_COMPILE}gcc
PLAT	:= vck190
COMMON	= sc_app
CFLAGS	= -I.
OBJS	= $(COMMON).o sc_$(PLAT).o
DEPS	= $(COMMON).h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(COMMON): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f $(COMMON) *.o
