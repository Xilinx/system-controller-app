PLAT	:= vck190
BIT	:= BIT
APP	= sc_app
APPD	= sc_appd
CFLAGS	= -I.
SRCDIR	= ../src
APP_OBJS	= $(APP).o sc_$(PLAT).o sc_$(BIT).o
APPD_OBJS	= $(APPD).o sc_$(PLAT).o
DEPS	= $(APP).h

all: $(APP) $(APPD)

%.o: $(SRCDIR)/%.c $(SRCDIR)/$(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP): $(APP_OBJS)
	$(CC) -o $@ $^ $(CFLAGS) -lm $(LDFLAGS)

$(APPD): $(APPD_OBJS)
	$(CC) -o $@ $^ $(CFLAGS) -lm $(LDFLAGS)

clean:
	rm -f $(APP) $(APPD) *.o
