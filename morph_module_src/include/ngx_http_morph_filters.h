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

#ifndef NGX_HTTP_MORPH_FILTERS_H
#define NGX_HTTP_MORPH_FILTERS_H

#include "ngx_http_morph_types.h"

// Apply gaussian blur with the given sigma.
// 지정한 sigma로 가우시안 블러를 적용한다.
vips::VImage morph_filters_apply_blur(vips::VImage image, double sigma);

// Convert image to grayscale.
// 이미지를 흑백으로 변환한다.
vips::VImage morph_filters_to_grayscale(vips::VImage image);

// Flatten transparent image against the given hex background color (e.g., "FF00FF").
// 투명 영역을 지정한 16진 배경색(예: "FF00FF")으로 채워 평탄화한다.
vips::VImage morph_filters_set_background(vips::VImage image, const char *color_hex);

// Adjust brightness (1.0 = original).
// 밝기를 조절한다 (1.0 = 원본).
vips::VImage morph_filters_apply_brightness(vips::VImage image, double brightness);

// Adjust contrast (1.0 = original).
// 대비를 조절한다 (1.0 = 원본).
vips::VImage morph_filters_apply_contrast(vips::VImage image, double contrast);

// Apply gaussian noise with the given sigma.
// 지정한 sigma로 가우시안 노이즈를 추가한다.
vips::VImage morph_filters_apply_noise(vips::VImage image, double sigma);

// Apply unsharp mask sharpen with the given sigma.
// 지정한 sigma로 언샤프 마스크 샤프닝을 적용한다.
vips::VImage morph_filters_apply_sharpen(vips::VImage image, double sigma);

// Full path of the watermark file: {root}/{service}/{watermark_path}.
// 워터마크 파일의 전체 경로: {root}/{service}/{watermark_path}.
std::string morph_filters_watermark_file(MorphOptions *options);

// Composite a watermark onto the image using gravity and opacity from options.
// options의 gravity와 opacity로 워터마크를 이미지에 합성한다.
vips::VImage morph_filters_apply_watermark(vips::VImage image, MorphOptions *options);

#endif