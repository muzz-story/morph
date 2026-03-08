# Nginx Morph Module

[English](README.md) | **한국어**

[![Build](../../actions/workflows/build.yml/badge.svg)](../../actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Nginx](https://img.shields.io/badge/nginx-1.26.x-green.svg)](https://nginx.org)
[![Libvips](https://img.shields.io/badge/libvips-8.x-green.svg)](https://libvips.github.io/libvips)

> 이 프로젝트는 이미지 리사이즈 처리를 위한 Nginx 모듈 기반 플랫폼입니다.
> 사람과 AI가 함께 만들었으며, 아직 성장 중인 프로젝트입니다.
> 사용 중 발견한 버그나 좋은 아이디어가 있다면 [Issues](../../issues)로 편하게 남겨주세요!

---

## 프로젝트 설명

**Nginx Morph Module**은 **Libvips** 기반의 고성능 이미지 실시간 처리(On-the-fly Image Processing)를 위한 Nginx 확장 모듈입니다.  
Nginx 내에서 이미지 리사이징, 크롭, 포맷 변환, 필터 적용 등의 작업을 직접 수행하며, 강력한 캐싱 및 멀티 소스 Failover 기능을 제공합니다.

---

## 프로젝트 폴더/파일 구조

```
morph/
├── morph_module_src/       # 모듈 C++ 소스 코드
│   ├── src/                # 핵심 소스 (.cpp)
│   ├── include/            # 헤더 파일 (.h)
│   └── config              # Nginx 모듈 config 스크립트
├── nginx_conf/             # Nginx 설정 파일 예시
│   ├── nginx.conf          # 메인 설정
│   └── morph.conf          # 서비스별 설정 (JSON)
├── docker/                 # Docker 빌드/테스트 환경
│   ├── Dockerfile
│   ├── docker-compose.yml
│   └── docker_build.sh
└── install.sh              # 리눅스 단독 설치용 빌드 스크립트
```

---

## 프로젝트 모듈 구조

- **ngx_http_morph_module.cpp**: 모듈 초기화, 설정 파싱, 요청 핸들링(Handler) 및 라이프사이클 관리.
- **ngx_http_morph_reader.cpp**: 이미지 소스 읽기 (로컬 파일 또는 원격 URL Fetch, Multi-source Failover).
- **ngx_http_morph_image.cpp**: 이미지 처리 파이프라인(캐시 확인 -> 로드 -> 변환 -> 저장) 관리.
- **ngx_http_morph_resizer.cpp**: 리사이징, 크롭, 회전 등 기하학적 변환 처리.
- **ngx_http_morph_filters.cpp**: 필터 효과 (Blur, Grayscale, Brightness 등) 처리.

---

## 프로젝트 기능

1.  **이미지 변환**: 리사이징(Width/Height), Smart Crop, 회전, 뒤집기.
2.  **화질 개선 및 필터**: Sharpen, Blur, Grayscale, Brightness/Contrast/Saturation 조정, 배경색 지정.
3.  **포맷 변환**: JPEG, PNG, WebP, GIF 지원 (자동 최적화).
4.  **강력한 캐싱**:
    - 변환된 이미지를 로컬 디스크에 캐싱.
    - **Lazy TTL**: 유효기간(TTL) 만료 시 자동 갱신.
    - **Purge**: HTTP DELETE 메서드를 이용한 즉시 캐시 삭제.
5.  **Multi-source Failover**: 원본 소스 서버가 여러 대일 경우, 실패 시 자동으로 다음 서버에서 이미지를 가져옵니다.
6.  **보안**: SSRF 방지, Path Traversal 차단, 관리자 IP만 Purge 허용 설정.

---

## 프로젝트 기능 사용법

### 1. URL 구조

```
http://[도메인]/morph/[서비스명]/[옵션]/[원본경로]
```

### 2. 옵션 예시 (`[옵션]` 부분)

- **기본 리사이징**: `200x300` (가로 200, 세로 300)
- **크롭 포함**: `200x300_C10,10,10,10` (상하좌우 10px 크롭 후 리사이징)
- **필터 적용**: `200x0_F(grayscale)_F(blur:1.5)` (세로 자동, 흑백 변환, 블러 1.5)
- **샤픈 필터**: `F(sharpen:1.0)` (선명하게)
- **워터마크**: `_W(path:logo.png,g:se,o:0.5)` (우측 하단, 투명도 50%)
- **포맷 변환**: `200x200_F(format:webp)`
- **GIF**: 별도 옵션 없이 애니메이션 GIF 자동 지원

### 3. 실제 요청 예시

- **Resize**: `/morph/myservice/500x500/images/photo.jpg`
- **Cache Purge (삭제)**:
  ```bash
  curl -X DELETE http://localhost/morph/myservice/500x500/images/photo.jpg
  ```

### 4. 설정 파일 (`conf/morph.conf`)

```json
{
  "service": {
    "myservice": {
      "ttl": 3600,
      "source": ["http://origin-primary.com", "http://origin-backup.com"]
    }
  }
}
```

---

## 다른 도구와의 비교

| 기능                    | **Morph**  | ngx_image_filter | imgproxy  |  Thumbor  |
| ----------------------- | :--------: | :--------------: | :-------: | :-------: |
| 아키텍처                | Nginx 모듈 |    Nginx 모듈    | 독립 서버 | 독립 서버 |
| 이미지 라이브러리       |  Libvips   |        GD        |  Libvips  |  Pillow   |
| 리사이즈 / 크롭         |     ✓      |        ✓         |     ✓     |     ✓     |
| Smart Crop              |     ✓      |        ✗         |     ✓     |     ✓     |
| 필터 (Blur, Sharpen 등) |     ✓      |      제한적      |     ✓     |     ✓     |
| 워터마크                |     ✓      |        ✗         |     ✓     |     ✓     |
| GIF 애니메이션          |     ✓      |        ✗         |     ✓     |     ✗     |
| 포맷 변환 (WebP 등)     |     ✓      |        ✓         |     ✓     |     ✓     |
| Multi-source Failover   |     ✓      |        ✗         |     ✗     |     ✗     |
| 디스크 캐시 (내장)      |     ✓      |        ✗         |     ✗     |     ✓     |
| SSRF 방어               |     ✓      |        ✗         |     ✓     |     ✓     |
| 설정 방식               |    JSON    |    nginx.conf    | 환경 변수 | 환경 변수 |
| 라이선스                | Apache 2.0 |       BSD        |    MIT    |    MIT    |

> Morph는 Nginx 모듈로서 별도의 프록시 없이 Nginx 내부에서 직접 동작하며,
> 멀티 소스 Failover와 내장 디스크 캐시를 동시에 제공하는 것이 강점입니다.

---

## Docker 사용 법 (테스트 환경)

Docker를 이용하면 복잡한 의존성 설치 없이 즉시 빌드 및 테스트가 가능합니다.

1.  **Docker 폴더로 이동**

    ```bash
    cd docker
    ```

2.  **컨테이너 실행 (자동 빌드)**

    ```bash
    docker-compose up --build
    ```

    - 소스 코드 변경 시 위 명령어를 다시 실행하면 재빌드됩니다.
    - 서버는 로컬 `8080` 포트로 뜹니다 (예: `http://localhost:8080/morph/...`).

3.  **테스트 URL 예시**

    서비스 `test` (소스: `https://picsum.photos`)가 기본 설정되어 있어 즉시 테스트 가능합니다.

    #### 리사이징

    | 기능                      | URL                                                           |
    | ------------------------- | ------------------------------------------------------------- |
    | 800×600 Smart Crop (중앙) | `http://localhost:8080/morph/test/800x600/1200/800`           |
    | 폭만 지정, 비율 유지      | `http://localhost:8080/morph/test/800x0/1200/800`             |
    | 높이만 지정, 비율 유지    | `http://localhost:8080/morph/test/0x400/1200/800`             |
    | Smart Crop — 위 기준      | `http://localhost:8080/morph/test/800x600_G(top)/1200/800`    |
    | Smart Crop — 아래 기준    | `http://localhost:8080/morph/test/800x600_G(bottom)/1200/800` |
    | Smart Crop — 왼쪽 기준    | `http://localhost:8080/morph/test/800x600_G(left)/1200/800`   |
    | 수동 크롭 (x,y,w,h)       | `http://localhost:8080/morph/test/C100,50,900,550/1200/800`   |

    #### 포맷 변환

    | 기능          | URL                                                                |
    | ------------- | ------------------------------------------------------------------ |
    | WebP 변환     | `http://localhost:8080/morph/test/800x600_F(format:webp)/1200/800` |
    | PNG 변환      | `http://localhost:8080/morph/test/800x600_F(format:png)/1200/800`  |
    | 품질 30% JPEG | `http://localhost:8080/morph/test/800x600_F(quality:30)/1200/800`  |

    #### 필터

    | 기능             | URL                                                                   |
    | ---------------- | --------------------------------------------------------------------- |
    | 흑백             | `http://localhost:8080/morph/test/800x600_F(grayscale)/1200/800`      |
    | 블러 (sigma 3.0) | `http://localhost:8080/morph/test/800x600_F(blur:3.0)/1200/800`       |
    | 샤픈 (sigma 2.0) | `http://localhost:8080/morph/test/800x600_F(sharpen:2.0)/1200/800`    |
    | 밝기 +50%        | `http://localhost:8080/morph/test/800x600_F(brightness:1.5)/1200/800` |
    | 대비 +50%        | `http://localhost:8080/morph/test/800x600_F(contrast:1.5)/1200/800`   |
    | 노이즈 추가      | `http://localhost:8080/morph/test/800x600_F(noise:10)/1200/800`       |
    | 90° 회전         | `http://localhost:8080/morph/test/F(rotate:90)/1200/800`              |
    | 좌우 반전        | `http://localhost:8080/morph/test/800x600_F(flip:h)/1200/800`         |
    | 상하 반전        | `http://localhost:8080/morph/test/800x600_F(flip:v)/1200/800`         |

    #### 복합 적용

    | 기능             | URL                                                                                   |
    | ---------------- | ------------------------------------------------------------------------------------- |
    | 흑백 + 블러      | `http://localhost:8080/morph/test/800x600_F(grayscale)_F(blur:2.0)/1200/800`          |
    | WebP + 샤픈      | `http://localhost:8080/morph/test/800x600_F(format:webp)_F(sharpen:1.5)/1200/800`     |
    | 밝기 + 대비 조합 | `http://localhost:8080/morph/test/800x600_F(brightness:1.2)_F(contrast:1.3)/1200/800` |

    #### 캐시 퍼지

    ```bash
    curl -X DELETE http://localhost:8080/morph/test/800x600/1200/800
    ```

---

## Docker 없이 단독 서버 세팅 법 (Linux)

### 1. 의존성 라이브러리 설치

Rocky Linux / RHEL 기준 필요한 패키지를 설치합니다.

```bash
# 기본 빌드 도구
dnf install -y gcc gcc-c++ make wget git tar openssl-devel pcre-devel zlib-devel libcurl-devel expat-devel

# Vips 및 이미지 관련 (Meson 빌드용)
dnf install -y meson ninja-build fftw-devel OpenEXR-devel libgsf-devel glib2-devel orc-devel libwebp-devel libjpeg-turbo-devel libexif-devel libtiff-devel librsvg2-devel cairo-devel lcms2-devel libimagequant-devel libpng-devel
```

### 2. CGIF 및 Libvips 빌드 (Shared Library)

```bash
# CGIF 설치
git clone https://github.com/dloebl/cgif
cd cgif
meson setup --prefix=/usr build
meson install -C build

# Libvips (8.18.0) 설치
wget https://github.com/libvips/libvips/releases/download/v8.18.0/vips-8.18.0.tar.gz
tar xf vips-8.18.0.tar.gz
cd vips-8.18.0
meson setup build --libdir=lib --buildtype=release --default-library shared -Ddeprecated=false -Dexamples=false
cd build
ninja && ninja install
ldconfig
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
```

### 3. 모듈 빌드 및 Nginx 설치

프로젝트 루트에서 제공된 `install.sh` 스크립트를 사용하거나 직접 configure를 수행합니다.

```bash
# Nginx 소스 폴더 안에서
./configure --prefix=/svc/morph/nginx \
            --with-threads --with-file-aio \
            --add-dynamic-module=/path/to/morph_module_src \
            # (나머지 라이브러리 옵션...)

make && make install
```

---

## 성능 테스트 방법

Docker 환경에서 간단하게 부하 테스트를 실행할 수 있습니다.

### 준비

```bash
# 컨테이너 실행 (포트 8080)
cd docker && docker-compose up --build -d

# morph.conf의 source에 실제 이미지를 응답하는 서버 URL을 지정하세요
```

### ab (Apache Benchmark)

```bash
# 100 requests, 동시 10
ab -n 100 -c 10 "http://localhost:8080/morph/myservice/500x500/images/photo.jpg"
```

### wrk

```bash
# 30초간, 4 threads, 50 connections
wrk -t4 -c50 -d30s "http://localhost:8080/morph/myservice/500x500/images/photo.jpg"
```

### 측정 항목

| 항목                 | 설명                                              |
| -------------------- | ------------------------------------------------- |
| **첫 번째 요청**     | 원본 fetch + 변환 + 캐시 저장 (Cold cache)        |
| **이후 요청**        | 디스크 캐시에서 직접 응답 (Warm cache)            |
| **TTL 만료 후 요청** | 백그라운드 갱신, 이전 캐시를 즉시 반환 (Lazy TTL) |

> Cold / Warm 캐시 응답 시간을 비교하면 캐시 효과를 직접 확인할 수 있습니다.

---

## 라이선스

Apache License 2.0 — [LICENSE](LICENSE) 파일을 참고하세요.
