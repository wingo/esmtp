#! /bin/sh

export  WANT_AUTOMAKE_1_6=1

aclocal \
&& automake --gnu --add-missing \
&& autoconf \
&& ./configure --enable-maintainer-mode

