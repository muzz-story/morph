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
vips::VImage morph_filters_apply_blur(vips::VImage image, double sigma);

// Convert image to grayscale.
vips::VImage morph_filters_to_grayscale(vips::VImage image);

// Flatten transparent image against the given hex background color (e.g., "FF00FF").
vips::VImage morph_filters_set_background(vips::VImage image, const char *color_hex);

// Adjust brightness (1.0 = original).
vips::VImage morph_filters_apply_brightness(vips::VImage image, double brightness);

// Adjust contrast (1.0 = original).
vips::VImage morph_filters_apply_contrast(vips::VImage image, double contrast);

// Apply gaussian noise with the given sigma.
vips::VImage morph_filters_apply_noise(vips::VImage image, int type, double sigma);

// Apply unsharp mask sharpen with the given sigma.
vips::VImage morph_filters_apply_sharpen(vips::VImage image, double sigma);

// Composite a watermark onto the image using gravity and opacity from options.
vips::VImage morph_filters_apply_watermark(vips::VImage image, MorphOptions *options);

#endif