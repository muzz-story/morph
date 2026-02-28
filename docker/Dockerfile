FROM rockylinux:9

# 1. Enable EPEL and CRB (Code Ready Builder) repositories
#    CRB is required for fftw-devel, orc-devel, OpenEXR-devel, etc.
RUN dnf install -y epel-release dnf-plugins-core && \
    dnf config-manager --enable crb && \
    dnf update -y

# 2. Install required build tools and core image libraries
#    These must all succeed — they are the minimum needed to build Nginx + Morph + Libvips.
RUN dnf install -y \
    gcc gcc-c++ make wget git tar \
    openssl-devel pcre-devel zlib-devel \
    libcurl-devel expat-devel \
    meson ninja-build \
    glib2-devel \
    libjpeg-turbo-devel libpng-devel \
    libwebp-devel libtiff-devel \
    libexif-devel lcms2-devel

# 3. Install optional image format libraries (best-effort — skip if unavailable)
#    libimagequant-devel is intentionally excluded (removed from EPEL 9).
RUN dnf install -y \
    fftw-devel orc-devel \
    librsvg2-devel cairo-devel \
    OpenEXR-devel libgsf-devel || true

# 4. Create source working directory
RUN mkdir -p /svc/src
WORKDIR /svc/src

# 5. Build CGIF (GIF animation support for Libvips)
RUN git clone https://github.com/dloebl/cgif && \
    cd cgif && \
    meson setup --prefix=/usr build && \
    meson install -C build && \
    cd ..

# 6. Build Libvips from source (latest stable release)
#    Uses GitHub's auto-generated source archive (archive/refs/tags/) which always exists,
#    unlike release asset uploads (releases/download/) which may be missing for some tags.
RUN VIPS_TAG=$(wget -qO- https://api.github.com/repos/libvips/libvips/releases/latest \
        | grep '"tag_name"' | head -1 | cut -d'"' -f4) && \
    VIPS_VER=${VIPS_TAG#v} && \
    echo "==> Building libvips ${VIPS_VER}" && \
    wget "https://github.com/libvips/libvips/archive/refs/tags/${VIPS_TAG}.tar.gz" \
        -O vips-${VIPS_VER}.tar.gz && \
    tar xf vips-${VIPS_VER}.tar.gz && \
    rm vips-${VIPS_VER}.tar.gz && \
    cd libvips-${VIPS_VER} && \
    meson setup build --libdir=lib --buildtype=release --default-library shared \
        -Ddeprecated=false -Dexamples=false -Dintrospection=disabled && \
    cd build && \
    ninja && \
    ninja install && \
    echo "/usr/local/lib" > /etc/ld.so.conf.d/local.conf && \
    ldconfig

# Set PKG_CONFIG_PATH so the Nginx build can find Libvips
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/share/pkgconfig
# Ensure the dynamic linker can find libvips at runtime
ENV LD_LIBRARY_PATH=/usr/local/lib

# 7. Download Nginx source
RUN wget https://nginx.org/download/nginx-1.26.2.tar.gz && tar -zxf nginx-1.26.2.tar.gz && rm nginx-1.26.2.tar.gz

# 8. Copy build script (strip Windows CRLF line endings if present)
COPY docker_build.sh /usr/src/docker_build.sh
RUN sed -i 's/\r$//' /usr/src/docker_build.sh && chmod +x /usr/src/docker_build.sh

# 9. Pre-create cache and log directories
#    chmod 777: nginx workers run as 'nobody' (default when built without --user),
#    so the cache directory must be world-writable for cache saves to succeed.
RUN mkdir -p /svc/morph/cache /svc/morph/nginx/logs && \
    chmod 777 /svc/morph/cache

WORKDIR /svc/src/nginx-1.26.2

CMD ["tail", "-f", "/dev/null"]
