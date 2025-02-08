BUILDDIR  = build
BINARY    = $(BUILDDIR)/gpio-monitor
CFLAGS    += -O2 -ffast-math -Werror-implicit-function-declaration -Werror=return-type
LDFLAGS   += -s

SRCS      = gpio-monitor.c gpio-base-sysfs.c cmd-execute.c conf-parse.c
_OBJS     = gpio-monitor.o gpio-base-sysfs.o cmd-execute.o conf-parse.o
OBJS      = $(patsubst %,$(BUILDDIR)/%,$(_OBJS))

all: mkbuild $(BINARY)

mkbuild: 
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o : %.c $(SRCS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINARY): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean

clean:
	rm -rf $(BUILDDIR)
