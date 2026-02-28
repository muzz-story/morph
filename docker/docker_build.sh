#!/bin/bash
set -e

# Build script executed inside the Docker container.
# Uses system-installed zlib, openssl, pcre (from dnf) — no bundled sources needed.

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

# Create log subdirectories required by nginx.conf
mkdir -p /svc/morph/nginx/logs/accesslog \
         /svc/morph/nginx/logs/errorlog
