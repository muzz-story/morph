FROM rockylinux:9

# 1. 패키지 업데이트 및 필요 도구 설치
# EPEL 저장소 추가 (libvips 등의 패키지를 위해)
RUN dnf install -y epel-release && \
    dnf update -y && \
    dnf install -y \
    gcc gcc-c++ make wget git tar \
    openssl-devel pcre-devel zlib-devel \
    libcurl-devel expat-devel \
    meson ninja-build \
    fftw-devel OpenEXR-devel libgsf-devel glib2-devel \
    orc-devel libwebp-devel libjpeg-turbo-devel libexif-devel \
    libtiff-devel librsvg2-devel cairo-devel lcms2-devel \
    libimagequant-devel libpng-devel

# 2. 작업 디렉토리 생성 (소스 위치)
RUN mkdir -p /svc/src
WORKDIR /svc/src

# 3. 라이브러리 소스 다운로드 및 빌드
# Dependencies (zlib, openSSL, PCRE)
RUN wget http://zlib.net/zlib-1.3.1.tar.gz && tar -zxvf zlib-1.3.1.tar.gz && rm zlib-1.3.1.tar.gz
RUN wget https://www.openssl.org/source/openssl-1.1.1w.tar.gz && tar -zxvf openssl-1.1.1w.tar.gz && rm openssl-1.1.1w.tar.gz
RUN wget https://downloads.sourceforge.net/project/pcre/pcre/8.45/pcre-8.45.tar.gz && tar -zxvf pcre-8.45.tar.gz && rm pcre-8.45.tar.gz

# CGIF Build (as requested)
RUN git clone https://github.com/dloebl/cgif && \
    cd cgif && \
    meson setup --prefix=/usr build && \
    meson install -C build && \
    cd ..

# Libvips Source Build (v8.18.0)
# Using Shared library to allow linking .so files
RUN wget https://github.com/libvips/libvips/releases/download/v8.18.0/vips-8.18.0.tar.gz && \
    tar xf vips-8.18.0.tar.gz && \
    rm vips-8.18.0.tar.gz && \
    cd vips-8.18.0 && \
    meson setup build --libdir=lib --buildtype=release --default-library shared -Ddeprecated=false -Dexamples=false -Dintrospection=disabled && \
    cd build && \
    ninja && \
    ninja install && \
    ldconfig

# Set PKG_CONFIG_PATH so Nginx build can find libvips
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/share/pkgconfig

# Nginx
RUN wget http://nginx.org/download/nginx-1.26.2.tar.gz && tar -zxvf nginx-1.26.2.tar.gz && rm nginx-1.26.2.tar.gz

# 4. 빌드 스크립트 복사
COPY docker_build.sh /usr/src/docker_build.sh
RUN chmod +x /usr/src/docker_build.sh

# 5. 캐시 및 로그 디렉토리
RUN mkdir -p /svc/morph/cache /svc/morph/nginx/logs

# 6. 작업 디렉토리 설정 (Build Root)
WORKDIR /svc/src/nginx-1.26.2

# 7. 기본 실행 명령 (컨테이너가 죽지 않고 대기하도록)
CMD ["tail", "-f", "/dev/null"]
