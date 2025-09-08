#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([hdapsd],[20250908],[https://github.com/linux-thinkpad/hdapsd])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/hdapsd.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile misc/Makefile])

AM_MAINTAINER_MODE([enable])

# Check for pkg-config/pkg.m4.
m4_pattern_forbid([^PKG_[A-Z_]+$], [pkg.m4 missing, please install pkg-config])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/time.h unistd.h syslog.h linux/input.h dirent.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday strerror uname])

# check for systemd
PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdsystemunitdir],
	AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
	[], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != xno; then
	AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

# check for udevdir
AC_ARG_WITH([udevdir],
	AS_HELP_STRING([--with-udevdir=DIR], [Directory for udev]),
	[], [with_udevdir=$($PKG_CONFIG --variable=udevdir udev)])
if test -z "$with_udevdir"; then
	with_udevdir="/lib/udev"
fi
AC_SUBST([udevdir], [$with_udevdir])

AC_ARG_ENABLE([libconfig],
	AS_HELP_STRING([--disable-libconfig], [Build without libconfig support]))

AS_IF([test "x$enable_libconfig" != "xno"], [
 PKG_CHECK_MODULES([LIBCONFIG], [libconfig],
  [AC_DEFINE([HAVE_LIBCONFIG], [1], [Have libconfig])
   have_libconfig=yes],
  [AC_MSG_NOTICE([libconfig was not found on your system, config file support will be disabled.])])
])
AM_CONDITIONAL(HAVE_LIBCONFIG, [test -n "$have_libconfig" -a "x$have_libconfig" = xyes ])

AC_OUTPUT
