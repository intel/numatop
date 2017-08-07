PREFIXDIR = /usr/local
BINDIR = /bin
MANDIR = /usr/share/man/man8
PROG = numatop
CC = gcc
LD = gcc
CFLAGS = -g -Wall -O2
LDFLAGS = -g
LDLIBS = -lncurses -lpthread -lnuma

NUMATOP_OBJS = numatop.o

COMMON_OBJS = cmd.o disp.o lwp.o page.o perf.o proc.o reg.o util.o \
	win.o ui_perf_map.o

OS_OBJS = os_cmd.o os_perf.o os_win.o node.o map.o os_util.o plat.o \
	pfwrapper.o sym.o os_page.o

TEST_PATH = ./test/mgen

ARCH := $(shell uname -m)

ifneq (,$(filter $(ARCH),ppc64le ppc64))
ARCH_PATH = ./powerpc
ARCH_OBJS = $(ARCH_PATH)/power8.o $(ARCH_PATH)/plat.o $(ARCH_PATH)/util.o \
	$(ARCH_PATH)/ui_perf_map.o

TEST_ARCH_PATH = $(TEST_PATH)/powerpc
else
ARCH_PATH = ./intel
ARCH_OBJS = $(ARCH_PATH)/wsm.o $(ARCH_PATH)/snb.o $(ARCH_PATH)/nhm.o \
	$(ARCH_PATH)/bdw.o $(ARCH_PATH)/skl.o $(ARCH_PATH)/plat.o \
	$(ARCH_PATH)/util.o $(ARCH_PATH)/ui_perf_map.o

TEST_ARCH_PATH = $(TEST_PATH)/intel
endif

TEST_PROG = $(TEST_PATH)/mgen
TEST_OBJS = $(TEST_PATH)/mgen.o
TEST_ARCH_OBJS = $(TEST_ARCH_PATH)/util.o

%.o: ./common/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: ./common/os/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(ARCH_PATH)/%.o: $(ARCH_PATH)/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(TEST_ARCH_PATH)/%o: $(TEST_ARCH_PATH)/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

all: $(PROG) test

# build numatop tool
$(PROG): $(NUMATOP_OBJS) $(COMMON_OBJS) $(OS_OBJS) $(ARCH_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(NUMATOP_OBJS) $(COMMON_OBJS) $(OS_OBJS) \
	$(ARCH_OBJS) $(LDLIBS)

# build mgen selftest
test: $(TEST_PROG)

$(TEST_PROG): $(TEST_OBJS) $(COMMON_OBJS) $(OS_OBJS) $(ARCH_OBJS) $(TEST_ARCH_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(TEST_OBJS) $(COMMON_OBJS) $(OS_OBJS) \
	$(ARCH_OBJS) $(TEST_ARCH_OBJS) $(LDLIBS)

install: $(PROG)
	install -m 0755 $(PROG) $(PREFIXDIR)$(BINDIR)/
	gzip -c numatop.8 > numatop.8.gz
	mv -f numatop.8.gz $(MANDIR)/

clean:
	rm -rf *.o $(ARCH_PATH)/*.o $(TEST_PATH)/*.o $(TEST_ARCH_PATH)/*.o \
	$(PROG) $(TEST_PROG)
