#!/bin/sh
set -e
autoreconf -I. -iv
./configure
MAKE=$(which gmake make | head -1)
$MAKE
