dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.53)
AC_INIT(esmtp, 0.4)
AC_CONFIG_SRCDIR(Makefile.am)
AM_INIT_AUTOMAKE(1.7 dist-bzip2)
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CPP

AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h)

dnl getopt needs this
AC_C_CONST

AM_PROG_LEX
AC_PROG_MAKE_SET
AC_PROG_YACC

YFLAGS=-d

dnl Check for pthreads
ACX_PTHREAD(
    LIBS="$PTHREAD_LIBS $LIBS"
    CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
    CC="$PTHREAD_CC",
    AC_MSG_WARN([No pthreads on this system.]))

dnl Check for libESMTP
AC_CHECK_LIB(
    esmtp, 
    smtp_start_session,
    LIBS="$LIBS -lesmtp",
    AC_MSG_ERROR([*** libESMTP library not found.]))

adl_FUNC_GETOPT_LONG

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
