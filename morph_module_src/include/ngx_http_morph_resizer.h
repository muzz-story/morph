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

#ifndef NGX_HTTP_MORPH_RESIZER_H
#define NGX_HTTP_MORPH_RESIZER_H

#include "ngx_http_morph_types.h"

// Apply fn to each frame of an animated (page-height) image, or once to a static image.
// 애니메이션(page-height) 이미지는 프레임마다, 정적 이미지는 한 번 fn을 적용한다.
vips::VImage morph_resizer_per_frame(vips::VImage image, const std::function<vips::VImage(vips::VImage)>& fn);

// Resize; keeps aspect ratio when one side is 0, stretches when both are given.
// 리사이즈. 한쪽이 0이면 비율을 유지하고, 둘 다 주면 늘려서 맞춘다.
vips::VImage morph_resizer_resize(vips::VImage image, int width, int height);

// Scale to cover, then crop based on gravity.
// 꽉 차게(cover) 스케일한 뒤 gravity 기준으로 자른다.
vips::VImage morph_resizer_resize_smart(vips::VImage image, int width, int height, int gravity);

// Crop the image to the specified area (cx, cy, cw, ch).
// 지정한 영역(cx, cy, cw, ch)으로 자른다.
vips::VImage morph_resizer_crop(vips::VImage image, int cx, int cy, int cw, int ch);

// Rotate the image by the given angle (degrees).
// 지정한 각도(도 단위)로 회전한다.
vips::VImage morph_resizer_rotate(vips::VImage image, double angle);

// Flip the image: direction 0 = vertical, 1 = horizontal.
// 이미지를 뒤집는다: 0 = 상하, 1 = 좌우.
vips::VImage morph_resizer_flip(vips::VImage image, int direction);

#endif