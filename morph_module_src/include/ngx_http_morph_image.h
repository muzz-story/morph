// Copyright 2025-2026 muzz
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NGX_HTTP_MORPH_IMAGE_H
#define NGX_HTTP_MORPH_IMAGE_H

#include "ngx_http_morph_types.h"

typedef enum {
    MORPH_IMG_UNKNOWN = 0,
    MORPH_IMG_JPEG,
    MORPH_IMG_PNG,
    MORPH_IMG_GIF,
    MORPH_IMG_WEBP
} MorphImageType;

// Run the full image pipeline: cache check -> fetch -> validate -> transform -> save.
// 전체 이미지 파이프라인 실행: 캐시 확인 -> 원본 가져오기 -> 검증 -> 변환 -> 저장.
ngx_int_t morph_image_process(MorphOptions *options, std::string *out_data, ngx_log_t *log, bool *is_cache_hit, time_t *last_modified);

// Identify image format by magic bytes. Sets out_type to MorphImageType.
// 매직 바이트로 이미지 포맷을 판별해 out_type에 MorphImageType을 넣는다.
ngx_int_t morph_image_validate_hex(void *data, size_t len, int *out_type);

// Compute the local cache file path from the given options.
// options로 로컬 캐시 파일 경로를 계산한다.
std::string morph_image_get_cache_path(MorphOptions *options);

#endif