#! /bin/sh

aclocal-1.9 \
&& autoheader \
&& automake-1.9 --gnu --add-missing \
&& autoconf \
&& CFLAGS="-Wall -pedantic -g" ./configure --enable-maintainer-mode

