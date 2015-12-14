PREFIXDIR = /usr/local
BINDIR = /bin
MANDIR = /usr/share/man/man8
PROG = numatop
CC = gcc
LD = gcc
CFLAGS = -g -Wall -O2
LDFLAGS = -g
LDLIBS = -lncurses -lpthread -lnuma

COMMON_OBJS = cmd.o disp.o lwp.o numatop.o page.o perf.o \
	proc.o reg.o util.o win.o

OS_OBJS = os_cmd.o os_perf.o os_win.o node.o map.o \
	os_util.o plat.o pfwrapper.o sym.o os_page.o

INTEL_OBJS = wsm.o snb.o nhm.o bdw.o

all: $(PROG)

$(PROG): $(COMMON_OBJS) $(OS_OBJS) $(INTEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(COMMON_OBJS) $(OS_OBJS) $(INTEL_OBJS) $(LDLIBS)

%.o: ./common/%.c ./common/include/*.h ./common/include/os/*.h
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: ./common/os/%.c ./common/include/*.h ./common/include/os/*.h
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: ./intel/%.c ./intel/include/*.h
	$(CC) $(CFLAGS) -o $@ -c $<

install: $(PROG)
	install -m 0755 $(PROG) $(PREFIXDIR)$(BINDIR)/
	gzip -c numatop.8 > numatop.8.gz
	mv -f numatop.8.gz $(MANDIR)/

clean:
	rm -rf *.o $(PROG)
