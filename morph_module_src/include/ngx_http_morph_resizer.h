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

// Resize the image to the specified dimensions.
vips::VImage morph_resizer_resize(vips::VImage image, int width, int height);

// Scale to cover, then crop based on gravity.
vips::VImage morph_resizer_resize_smart(vips::VImage image, int width, int height, int gravity);

// Crop the image to the specified area (cx, cy, cw, ch).
vips::VImage morph_resizer_crop(vips::VImage image, int cx, int cy, int cw, int ch);

// Rotate the image by the given angle (degrees).
vips::VImage morph_resizer_rotate(vips::VImage image, double angle);

// Flip the image: direction 0 = vertical, 1 = horizontal.
vips::VImage morph_resizer_flip(vips::VImage image, int direction);

#endif