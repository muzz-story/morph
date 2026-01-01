# Nginx Morph Module

Nginx module for real-time image processing using Libvips.
Libvips를 활용하여 Nginx 상에서 실시간으로 이미지를 변환하고 제공하는 모듈입니다.

## Module Structure (모듈 구조)

이 프로젝트는 기능별로 모듈화된 구조를 가지고 있습니다.

### 1. `ngx_http_morph_module`
- **Role**: Entry Point & Handler.
- **Description**: 
  - Nginx 초기 기능을 구현하고 사용자가 요청한 처리에 대한 handler 처리를 담당합니다.
  - Handler 기능을 통해 다른 모듈의 함수를 호출하여 최종적으로 사용자에게 변환된 이미지 파일을 전달합니다.

### 2. `ngx_http_morph_image`
- **Role**: Image Processing Orchestrator & Validator.
- **Description**: 
  - `reader`를 통해 파일을 읽어 온 후, 파일의 Hex(Magic Number)를 확인하여 파일 정상 여부를 검증합니다.
  - 검증 후 리사이징 및 필터링 작업을 순차적으로 수행하도록 관리합니다.

### 3. `ngx_http_morph_reader`
- **Role**: Image Source Loader.
- **Description**: 
  - 이미지 원본 파일을 가져오는 기능을 담당합니다.
  - 캐시가 되어있으면 캐시 파일을 로드하고, 없으면 로컬 원본이나 요청된 외부 사이트(HTTP/HTTPS)에서 이미지를 가져옵니다.

### 4. `ngx_http_morph_resizer`
- **Role**: Geometric Transformations.
- **Description**: 
  - 이미지 리사이즈 기능을 담당합니다.
  - Resize(크기 조절), Crop(자르기), Rotate(회전), Flip(반전) 등의 기하학적 변환을 수행합니다.

### 5. `ngx_http_morph_filters`
- **Role**: Image Filters.
- **Description**: 
  - 이미지 필터 기능을 관리합니다.
  - 다음 필터 기능을 포함합니다:
    - 배경색 변경 (Background Color)
    - Blur 처리 (Blur)
    - Brightness 처리 (Brightness)
    - Contrast 처리 (Contrast)
    - Format 변경 (Format Change)
    - Grayscale 처리 (Grayscale)
    - Noise 처리 (Noise)
    - JPG 품질 변경 (Quality)
    - Rotate 처리 (Rotate - *Note: 기하학적 변환이지만 편의상 필터로 분류될 수 있음*)

## Build & Installation

Please refer to `install.sh` for build instructions.
`install.sh` 스크립트를 참조하여 빌드를 진행하세요.
