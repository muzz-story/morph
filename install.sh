#!/bin/bash
set -e

# Standalone build script for Linux.
# Run this from inside the nginx source directory.
# Prerequisites: openssl-devel, pcre-devel, zlib-devel must be installed via dnf/apt.
# See README.md for full dependency installation instructions.

./configure --prefix=/svc/morph/nginx \
            --without-http_charset_module \
            --with-http_realip_module \
            --with-http_ssl_module \
            --with-threads \
            --with-file-aio \
            --with-cc-opt=-O3 \
            --add-dynamic-module=../morph_module_src

make -j$(nproc)
make install
