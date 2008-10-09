#! /bin/sh

aclocal \
&& autoheader \
&& automake --gnu --add-missing \
&& autoconf \
&& CFLAGS="-Wall -pedantic -g" ./configure --enable-maintainer-mode

