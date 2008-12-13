#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(hdapsd, 20081004, hdaps-devel@lists.sourceforge.net)
AM_INIT_AUTOMAKE(hdapsd, 20081004)
AC_CONFIG_SRCDIR([src/hdapsd.c])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME

# Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([gettimeofday strerror uname])

AC_OUTPUT
