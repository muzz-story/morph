# Nginx Morph Module

[![Build](../../actions/workflows/build.yml/badge.svg)](../../actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Nginx](https://img.shields.io/badge/nginx-1.26.x-green.svg)](https://nginx.org)
[![Libvips](https://img.shields.io/badge/libvips-8.x-green.svg)](https://libvips.github.io/libvips)

> This project is an Nginx module platform for on-the-fly image processing.
> Built together by a human and AI — still growing.
> Found a bug or have a good idea? Feel free to open an [Issue](../../issues)!

---

## Overview

**Nginx Morph Module** is a high-performance, on-the-fly image processing Nginx extension module built on **Libvips**.
It performs image resizing, cropping, format conversion, and filter effects directly inside Nginx, with built-in disk caching and multi-source failover.

---

## Project Structure

```
morph/
├── morph_module_src/       # C++ module source
│   ├── src/                # Core source files (.cpp)
│   ├── include/            # Header files (.h)
│   └── config              # Nginx module config script
├── nginx_conf/             # Nginx configuration examples
│   ├── nginx.conf          # Main config
│   └── morph.conf          # Service config (JSON)
├── docker/                 # Docker build/test environment
│   ├── Dockerfile
│   ├── docker-compose.yml
│   └── docker_build.sh
└── install.sh              # Standalone Linux build script
```

---

## Module Internals

| File | Role |
|---|---|
| `ngx_http_morph_module.cpp` | Module init, config parsing, request handling, lifecycle |
| `ngx_http_morph_reader.cpp` | Image source reading (local file or remote URL fetch, multi-source failover) |
| `ngx_http_morph_image.cpp` | Image processing pipeline (cache check → load → transform → save) |
| `ngx_http_morph_resizer.cpp` | Resize, crop, rotate, flip (geometric transforms) |
| `ngx_http_morph_filters.cpp` | Filter effects (blur, grayscale, brightness, sharpen, watermark, etc.) |

---

## Features

1. **Image Transform** — Resize (width/height), Smart Crop, rotate, flip
2. **Filters** — Sharpen, blur, grayscale, brightness/contrast/saturation, background color
3. **Format Conversion** — JPEG, PNG, WebP, GIF (auto-detect, auto-optimize)
4. **Caching**
   - Transformed images cached to local disk
   - **Lazy TTL**: expired cache is refreshed automatically on next request
   - **Purge**: instant cache delete via HTTP `DELETE`
5. **Multi-source Failover** — If the primary origin fails, automatically tries the next source
6. **Security** — SSRF protection, path traversal blocking, purge restricted to admin IPs

---

## Usage

### URL Structure

```
http://[domain]/morph/[service]/[options]/[origin-path]
```

### Option Examples

| Option | Description |
|---|---|
| `200x300` | Resize to 200×300 |
| `200x300_C10,10,10,10` | Crop 10px each side, then resize |
| `200x0_F(grayscale)_F(blur:1.5)` | Auto height, grayscale + blur |
| `F(sharpen:1.0)` | Sharpen |
| `_W(path:logo.png,g:se,o:0.5)` | Watermark bottom-right, 50% opacity |
| `200x200_F(format:webp)` | Convert to WebP |

### Request Examples

```bash
# Resize
GET /morph/myservice/500x500/images/photo.jpg

# Cache purge
curl -X DELETE http://localhost/morph/myservice/500x500/images/photo.jpg
```

### Service Config (`nginx_conf/morph.conf`)

```json
{
  "service": {
    "myservice": {
      "ttl": 3600,
      "source": ["http://origin-primary.example.com", "http://origin-backup.example.com"]
    },
    "static": {
      "ttl": -1,
      "source": ["http://static.example.com"]
    }
  }
}
```

---

## Comparison

| Feature | **Morph** | ngx_image_filter | imgproxy | Thumbor |
|---|:---:|:---:|:---:|:---:|
| Architecture | Nginx module | Nginx module | Standalone server | Standalone server |
| Image library | Libvips | GD | Libvips | Pillow |
| Resize / Crop | ✓ | ✓ | ✓ | ✓ |
| Smart Crop | ✓ | ✗ | ✓ | ✓ |
| Filters (Blur, Sharpen, etc.) | ✓ | Limited | ✓ | ✓ |
| Watermark | ✓ | ✗ | ✓ | ✓ |
| GIF animation | ✓ | ✗ | ✓ | ✗ |
| Format conversion (WebP, etc.) | ✓ | ✓ | ✓ | ✓ |
| Multi-source failover | ✓ | ✗ | ✗ | ✗ |
| Built-in disk cache | ✓ | ✗ | ✗ | ✓ |
| SSRF protection | ✓ | ✗ | ✓ | ✓ |
| Configuration | JSON | nginx.conf | env vars | env vars |
| License | Apache 2.0 | BSD | MIT | MIT |

> Morph runs as an Nginx module — no extra proxy needed — and uniquely combines
> multi-source failover with a built-in disk cache in a single deployment.

---

## Quick Start (Docker)

```bash
cd docker
docker-compose up --build
```

The server starts on port `8080`. Rebuild anytime by re-running the same command.

### Test URLs

Service `test` (source: `https://picsum.photos`) is pre-configured — no extra setup needed.

#### Resize

| Feature | URL |
|---|---|
| 800×600 Smart Crop (center) | `http://localhost:8080/morph/test/800x600/1200/800` |
| Width only, aspect preserved | `http://localhost:8080/morph/test/800x0/1200/800` |
| Height only, aspect preserved | `http://localhost:8080/morph/test/0x400/1200/800` |
| Smart Crop — top gravity | `http://localhost:8080/morph/test/800x600_G(top)/1200/800` |
| Smart Crop — bottom gravity | `http://localhost:8080/morph/test/800x600_G(bottom)/1200/800` |
| Smart Crop — left gravity | `http://localhost:8080/morph/test/800x600_G(left)/1200/800` |
| Manual crop (x,y,w,h) | `http://localhost:8080/morph/test/C100,50,900,550/1200/800` |

#### Format

| Feature | URL |
|---|---|
| Convert to WebP | `http://localhost:8080/morph/test/800x600_F(format:webp)/1200/800` |
| Convert to PNG | `http://localhost:8080/morph/test/800x600_F(format:png)/1200/800` |
| JPEG quality 30 | `http://localhost:8080/morph/test/800x600_F(quality:30)/1200/800` |

#### Filters

| Feature | URL |
|---|---|
| Grayscale | `http://localhost:8080/morph/test/800x600_F(grayscale)/1200/800` |
| Blur (sigma 3.0) | `http://localhost:8080/morph/test/800x600_F(blur:3.0)/1200/800` |
| Sharpen (sigma 2.0) | `http://localhost:8080/morph/test/800x600_F(sharpen:2.0)/1200/800` |
| Brightness +50% | `http://localhost:8080/morph/test/800x600_F(brightness:1.5)/1200/800` |
| Contrast +50% | `http://localhost:8080/morph/test/800x600_F(contrast:1.5)/1200/800` |
| Add noise | `http://localhost:8080/morph/test/800x600_F(noise:10)/1200/800` |
| Rotate 90° | `http://localhost:8080/morph/test/F(rotate:90)/1200/800` |
| Flip horizontal | `http://localhost:8080/morph/test/800x600_F(flip:h)/1200/800` |
| Flip vertical | `http://localhost:8080/morph/test/800x600_F(flip:v)/1200/800` |

#### Combinations

| Feature | URL |
|---|---|
| Grayscale + Blur | `http://localhost:8080/morph/test/800x600_F(grayscale)_F(blur:2.0)/1200/800` |
| WebP + Sharpen | `http://localhost:8080/morph/test/800x600_F(format:webp)_F(sharpen:1.5)/1200/800` |
| Brightness + Contrast | `http://localhost:8080/morph/test/800x600_F(brightness:1.2)_F(contrast:1.3)/1200/800` |

#### Cache Purge

```bash
curl -X DELETE http://localhost:8080/morph/test/800x600/1200/800
```

---

## Linux Installation (without Docker)

### 1. Install Dependencies

Rocky Linux / RHEL:

```bash
dnf install -y gcc gcc-c++ make wget git tar openssl-devel pcre-devel zlib-devel libcurl-devel expat-devel
dnf install -y meson ninja-build fftw-devel OpenEXR-devel libgsf-devel glib2-devel orc-devel \
    libwebp-devel libjpeg-turbo-devel libexif-devel libtiff-devel librsvg2-devel \
    cairo-devel lcms2-devel libimagequant-devel libpng-devel
```

### 2. Build CGIF and Libvips

```bash
# CGIF
git clone https://github.com/dloebl/cgif
cd cgif && meson setup --prefix=/usr build && meson install -C build && cd ..

# Libvips 8.18.0
wget https://github.com/libvips/libvips/releases/download/v8.18.0/vips-8.18.0.tar.gz
tar xf vips-8.18.0.tar.gz && cd vips-8.18.0
meson setup build --libdir=lib --buildtype=release --default-library shared \
    -Ddeprecated=false -Dexamples=false
cd build && ninja && ninja install && ldconfig
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
```

### 3. Build Nginx + Morph Module

```bash
# Inside the Nginx source directory
./configure --prefix=/svc/morph/nginx \
            --with-threads --with-file-aio \
            --add-dynamic-module=/path/to/morph_module_src

make && make install
```

Or use the provided `install.sh` from the project root.

---

## Benchmarking

You can run load tests against the Docker environment in a few commands.

### Setup

```bash
# Start the container (port 8080)
cd docker && docker-compose up --build -d

# Make sure morph.conf points to a real origin server that serves images
```

### ab (Apache Benchmark)

```bash
# 100 requests, concurrency 10
ab -n 100 -c 10 "http://localhost:8080/morph/myservice/500x500/images/photo.jpg"
```

### wrk

```bash
# 30 seconds, 4 threads, 50 connections
wrk -t4 -c50 -d30s "http://localhost:8080/morph/myservice/500x500/images/photo.jpg"
```

### What to Measure

| Scenario | Behavior |
|---|---|
| **First request** | Origin fetch + transform + cache write (Cold cache) |
| **Subsequent requests** | Served directly from disk cache (Warm cache) |
| **After TTL expiry** | Stale cache returned immediately; refreshed in background (Lazy TTL) |

> Compare cold vs. warm cache response times to see the caching benefit directly.

---

## License

Apache License 2.0 — see [LICENSE](LICENSE) for details.
