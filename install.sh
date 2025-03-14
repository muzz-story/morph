#!/bin/bash

./configure --prefix=/svc/morph/nginx \
            --without-http_charset_module \
            --with-http_realip_module \
            --with-http_ssl_module \
            --with-threads \
            --with-file-aio \
            --with-cc-opt=-O3 \
            --with-zlib=../zlib-1.3.1 \
            --with-openssl=../openssl-1.1.1w \
            --with-pcre=../pcre-8.45 \
            --add-dynamic-module=../morph_module_src
