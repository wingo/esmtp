dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.53)
AC_INIT(esmtp, 0.5.1)
AC_CONFIG_SRCDIR(Makefile.am)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE(1.7 dist-bzip2)
AM_MAINTAINER_MODE

AC_PROG_CC

AC_HEADER_STDC

dnl getopt needs this
AC_C_CONST

AC_PROG_LEX
AC_PROG_LN_S
AC_PROG_YACC

dnl Check for libESMTP
AC_MSG_CHECKING(for libESMTP)
if libesmtp-config --version > /dev/null 2>&1
then
	AC_MSG_RESULT(yes)
	CFLAGS="$CFLAGS `libesmtp-config --cflags`"
	LIBS="$LIBS `libesmtp-config --libs`"
else
	AC_MSG_RESULT(no)
	AC_MSG_ERROR(libESMTP library not found)
fi

adl_FUNC_GETOPT_LONG

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
