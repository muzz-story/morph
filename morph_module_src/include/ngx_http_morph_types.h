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

#ifndef NGX_HTTP_MORPH_TYPES_H
#define NGX_HTTP_MORPH_TYPES_H

#include "std.h"

// Gravity Constants
#define MORPH_GRAVITY_CENTER 0
#define MORPH_GRAVITY_TOP    1
#define MORPH_GRAVITY_BOTTOM 2
#define MORPH_GRAVITY_LEFT   3
#define MORPH_GRAVITY_RIGHT  4
#define MORPH_GRAVITY_TOP_LEFT 5
#define MORPH_GRAVITY_TOP_RIGHT 6
#define MORPH_GRAVITY_BOTTOM_LEFT 7
#define MORPH_GRAVITY_BOTTOM_RIGHT 8

typedef struct {
    int width = 0;
    int height = 0;
    bool debug = false;
    
    // Crop Settings
    bool has_crop = false; // Manual Crop (Cx, Cy, Cw, Ch)
    int cx = 0;
    int cy = 0;
    int cw = 0;
    int ch = 0;

    // Smart Crop / Resize Settings
    int gravity = MORPH_GRAVITY_CENTER; // For "Cover" resize strategy if no manual crop

    // Filters
    std::string bg_color;
    double blur_sigma = 0.0;
    std::string format;
    bool grayscale = false;
    int quality = 0;
    double rotate_angle = 0.0;
    
    bool flip = false;
    int flip_dir = 0; // 0: vertical, 1: horizontal
    double brightness = 1.0;
    double contrast = 1.0;
    double noise_sigma = 0.0;
    int noise_type = 0;

    // Sharpen Filter
    double sharpen_sigma = 0.0;

    // Watermark
    std::string watermark_path;
    int watermark_gravity = MORPH_GRAVITY_CENTER;
    double watermark_opacity = 1.0;
    int watermark_x_offset = 0;
    int watermark_y_offset = 0;
    
    std::string source_path;
    std::string service_name;
    std::string document_root;
    std::string raw_options;
} MorphOptions;

struct MorphServiceConfig {
    std::vector<std::string> sources;
    int ttl; // seconds, -1 for no expiry
};

#endif // NGX_HTTP_MORPH_TYPES_H
