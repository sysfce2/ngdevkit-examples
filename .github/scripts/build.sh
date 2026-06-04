#!/bin/sh
set -e
autoreconf -I. -iv
./configure
make
