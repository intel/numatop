#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([numatop], [v2.3], [zhengjun.xing@intel.com])
AM_INIT_AUTOMAKE([-Wno-portability no-dist-gzip dist-xz foreign subdir-objects])
AC_CONFIG_SRCDIR([common/numatop.c])
AC_CONFIG_HEADERS([config.h])
LT_INIT
AC_CONFIG_MACRO_DIRS([m4])

AC_CANONICAL_HOST
AM_CONDITIONAL(CPU_X86, test "x$host_cpu" = "xx86_64" -o "x$host_cpu" = "xi686")
AM_CONDITIONAL(CPU_PPC, test "x$host_cpu" = "xpowerpc64" -o "x$host_cpu" = "xpowerpc64le")

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([numa], [numa_free], [], [ AC_MSG_ERROR([numactl-devel or libnuma-dev(el) is required but was not found]) exit -1])
AC_CHECK_LIB([pthread], [pthread_create])

PKG_CHECK_MODULES([CHECK], [check])

PKG_CHECK_MODULES([NCURSES], [ncursesw ncurses], [LIBS="$LIBS $ncurses_LIBS"], [
	AC_SEARCH_LIBS([delwin], [ncursesw ncurses], [], [
		AC_MSG_ERROR([ncurses is required but was not found])
	], [])
])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h locale.h stddef.h stdint.h stdlib.h string.h strings.h sys/ioctl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([getpagesize gettimeofday memset munmap select strcasecmp strchr strcspn strstr strtol strtoull])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
