#! /bin/sh

aclocal-1.6 \
&& automake-1.6 --gnu --add-missing \
&& autoconf \
&& ./configure --enable-maintainer-mode

