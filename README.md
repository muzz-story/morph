# Nginx Morph Module

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
*   **ngx_http_morph_module.cpp**: 모듈 초기화, 설정 파싱, 요청 핸들링(Handler) 및 라이프사이클 관리.
*   **ngx_http_morph_reader.cpp**: 이미지 소스 읽기 (로컬 파일 또는 원격 URL Fetch, Multi-source Failover).
*   **ngx_http_morph_image.cpp**: 이미지 처리 파이프라인(캐시 확인 -> 로드 -> 변환 -> 저장) 관리.
*   **ngx_http_morph_resizer.cpp**: 리사이징, 크롭, 회전 등 기하학적 변환 처리.
*   **ngx_http_morph_filters.cpp**: 필터 효과 (Blur, Grayscale, Brightness 등) 처리.

---

## 프로젝트 기능
1.  **이미지 변환**: 리사이징(Width/Height), Smart Crop, 회전, 뒤집기.
2.  **화질 개선 및 필터**: Sharpen, Blur, Grayscale, Brightness/Contrast/Saturation 조정, 배경색 지정.
3.  **포맷 변환**: JPEG, PNG, WebP, GIF 지원 (자동 최적화).
4.  **강력한 캐싱**:
    *   변환된 이미지를 로컬 디스크에 캐싱.
    *   **Lazy TTL**: 유효기간(TTL) 만료 시 자동 갱신.
    *   **Purge**: HTTP DELETE 메서드를 이용한 즉시 캐시 삭제.
5.  **Multi-source Failover**: 원본 소스 서버가 여러 대일 경우, 실패 시 자동으로 다음 서버에서 이미지를 가져옵니다.
6.  **보안**: SSRF 방지, Path Traversal 차단, 관리자 IP만 Purge 허용 설정.

---

## 프로젝트 기능 사용법

### 1. URL 구조
```
http://[도메인]/morph/[서비스명]/[옵션]/[원본경로]
```

### 2. 옵션 예시 (`[옵션]` 부분)
*   **기본 리사이징**: `200x300` (가로 200, 세로 300)
*   **크롭 포함**: `200x300_C10,10,10,10` (상하좌우 10px 크롭 후 리사이징)
*   **필터 적용**: `200x0_F(grayscale)_F(blur:1.5)` (세로 자동, 흑백 변환, 블러 1.5)
*   **포맷 변환**: `200x200_F(format:webp)`

### 3. 실제 요청 예시
*   **Resize**: `/morph/myservice/500x500/images/photo.jpg`
*   **Cache Purge (삭제)**:
    ```bash
    curl -X DELETE http://localhost/morph/myservice/500x500/images/photo.jpg
    ```

### 4. 설정 파일 (`conf/morph.conf`)
```json
{
  "service": {
    "myservice": {
      "ttl": 3600,
      "source": [ "http://origin-primary.com", "http://origin-backup.com" ]
    }
  }
}
```

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
    *   소스 코드 변경 시 위 명령어를 다시 실행하면 재빌드됩니다.
    *   서버는 로컬 `8080` 포트로 뜹니다 (예: `http://localhost:8080/morph/...`).

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
