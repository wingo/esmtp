dnl Process this file with autoconf to produce a configure script.

AC_INIT(Makefile.am)
AM_INIT_AUTOMAKE(esmtp, 0.2)

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

dnl Check for libESMTP
AC_CHECK_LIB(esmtp, smtp_start_session, 
    LIBS="$LIBS -lesmtp -lpthread",
    AC_MSG_ERROR([*** libESMTP library not found.]),
    -lpthread)

AC_OUTPUT([Makefile])
