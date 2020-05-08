PLAT	:= vck190
BIT	:= BIT
COMMON	= sc_app
CFLAGS	= -I.
SRCDIR	= ../src
OBJS	= $(COMMON).o sc_$(PLAT).o sc_$(BIT).o
DEPS	= $(COMMON).h

%.o: $(SRCDIR)/%.c $(SRCDIR)/$(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(COMMON): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) -lm $(LDFLAGS)

clean:
	rm -f $(COMMON) *.o
