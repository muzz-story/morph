#!/bin/bash

# Docker 내부에서 실행되는 빌드 스크립트입니다.
# 시스템 라이브러리(zlib-devel, openssl-devel, pcre-devel)를 사용하므로
# 기존 install.sh의 --with-zlib=.. 등의 옵션을 제거하고 심플하게 구성합니다.

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

make -j$(nproc)
make install
