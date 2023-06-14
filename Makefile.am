
EXTRA_DIST = \
	README.md \
	AUTHORS \
	COPYING \
	kernel_patches/0001-perf-x86-Widen-Haswell-OFFCORE-mask.patch \
	$(TESTS)

dist_man_MANS = \
	numatop.8

AM_CFLAGS = -fPIC -O2 -g -Wall -W -Wformat-security -D_FORTIFY_SOURCE=2 -fno-common -std=gnu99
ACLOCAL_AMFLAGS = -I m4

noinst_LTLIBRARIES = libnumatop.la
libnumatop_la_SOURCES = \
	common/include/os/linux/perf_event.h \
	common/include/os/map.h \
	common/include/os/node.h \
	common/include/os/os_cmd.h \
	common/include/os/os_page.h \
	common/include/os/os_perf.h \
	common/include/os/os_types.h \
	common/include/os/os_util.h \
	common/include/os/os_win.h \
	common/include/os/pfwrapper.h \
	common/include/os/plat.h \
	common/include/os/sym.h \
	common/include/cmd.h \
	common/include/disp.h \
	common/include/lwp.h \
	common/include/page.h \
	common/include/perf.h \
	common/include/proc.h \
	common/include/reg.h \
	common/include/types.h \
	common/include/ui_perf_map.h \
	common/include/util.h \
	common/include/win.h \
	common/os/map.c \
	common/os/node.c \
	common/os/os_cmd.c \
	common/os/os_page.c \
	common/os/os_perf.c \
	common/os/os_util.c \
	common/os/os_win.c \
	common/os/pfwrapper.c \
	common/os/plat.c \
	common/os/sym.c \
	common/cmd.c \
	common/disp.c \
	common/lwp.c \
	common/page.c \
	common/perf.c \
	common/proc.c \
	common/reg.c \
	common/ui_perf_map.c \
	common/util.c \
	common/win.c

if CPU_X86
libnumatop_la_SOURCES += \
	x86/include/bdw.h \
	x86/include/nhm.h \
	x86/include/skl.h \
	x86/include/snb.h \
	x86/include/types.h \
	x86/include/util.h \
	x86/include/wsm.h \
	x86/include/zen.h \
	x86/bdw.c \
	x86/nhm.c \
	x86/plat.c \
	x86/skl.c \
	x86/snb.c \
	x86/ui_perf_map.c \
	x86/util.c \
	x86/wsm.c \
	x86/zen.c
endif

if CPU_PPC
libnumatop_la_SOURCES += \
	powerpc/include/power8.h \
	powerpc/include/power9.h \
	powerpc/include/types.h \
	powerpc/plat.c \
	powerpc/power8.c \
	powerpc/power9.c \
	powerpc/ui_perf_map.c \
	powerpc/util.c
endif

bin_PROGRAMS = numatop
noinst_PROGRAMS = mgen

numatop_CFLAGS = $(NCURSES_CFLAGS)
numatop_LDADD = libnumatop.la $(NCURSES_LIBS)
numatop_SOURCES = common/numatop.c

mgen_CFLAGS = $(NCURSES_CFLAGS)
mgen_LDADD = libnumatop.la $(NCURSES_LIBS)
mgen_SOURCES = \
	test/mgen/include/util.h \
	test/mgen/mgen.c

if CPU_PPC
mgen_SOURCES += \
	test/mgen/powerpc/util.c
endif
if CPU_X86
mgen_SOURCES += \
	test/mgen/x86/util.c
endif

TESTS = test/mgen.01.sh test/mgen.02.sh
