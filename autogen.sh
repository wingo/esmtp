#! /bin/sh

aclocal \
&& automake --gnu --add-missing \
&& autoconf \
&& ./configure --enable-maintainer-mode

