#! /bin/sh

aclocal-1.7 \
&& autoheader \
&& automake-1.7 --gnu --add-missing \
&& autoconf \
&& CFLAGS="-Wall -pedantic -g" ./configure --enable-maintainer-mode

