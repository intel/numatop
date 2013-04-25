PREFIXDIR = /usr/local
BINDIR = /bin
MANDIR = /usr/share/man/man8
PROG = numatop
CC = gcc
LD = gcc
CFLAGS = -g -Wall -O2
LDLIBS = -lncurses -lpthread -lnuma

COMMON_OBJS = cmd.o disp.o lwp.o node.o numatop.o page.o perf.o \
	plat.o proc.o reg.o util.o win.o pfwrapper.o sym.o \
	map.o

INTEL_OBJS = wsm.o snb.o nhm.o

all: $(PROG)

$(PROG): $(COMMON_OBJS) $(INTEL_OBJS)
	$(LD) -o $@ $(COMMON_OBJS) $(INTEL_OBJS) $(LDLIBS)

%.o: ./common/%.c ./common/include/*.h
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: ./intel/%.c ./intel/include/*.h
	$(CC) $(CFLAGS) -o $@ -c $<

install: $(PROG)
	install -m 0755 $(PROG) $(PREFIXDIR)$(BINDIR)/
	gzip -c numatop.8 > numatop.8.gz
	mv -f numatop.8.gz $(MANDIR)/

clean:
	rm -rf *.o $(PROG)
