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

#include "std.h"
#include "ngx_http_morph_filters.h"

// Apply gaussian blur with the given sigma.
vips::VImage morph_filters_apply_blur(vips::VImage image, double sigma) {
    if (sigma <= 0.0) return image;
    return image.gaussblur(sigma);
}

// Convert image to grayscale.
vips::VImage morph_filters_to_grayscale(vips::VImage image) {
    return image.colourspace(VIPS_INTERPRETATION_B_W);
}

// Flatten transparent image against the given hex background color (e.g., "FF00FF").
vips::VImage morph_filters_set_background(vips::VImage image, const char *color_hex) {
    std::vector<double> bg_color;
    if (color_hex && strlen(color_hex) == 6) {
        int r, g, b;
        if (sscanf(color_hex, "%02x%02x%02x", &r, &g, &b) == 3) {
            bg_color.push_back(r);
            bg_color.push_back(g);
            bg_color.push_back(b);
        } else {
             bg_color = {255, 255, 255};
        }
    } else {
        bg_color = {255, 255, 255};
    }
    
    return image.flatten(vips::VImage::option()->set("background", bg_color));
}

// Adjust brightness (1.0 = original).
vips::VImage morph_filters_apply_brightness(vips::VImage image, double brightness) {
    return image.linear({brightness}, {0});
}

// Adjust contrast (1.0 = original). Pivots around mid-grey (128).
vips::VImage morph_filters_apply_contrast(vips::VImage image, double contrast) {
    return image.linear({contrast}, {128.0 * (1.0 - contrast)});
}

// Apply gaussian noise with the given sigma.
vips::VImage morph_filters_apply_noise(vips::VImage image, int type, double sigma) {
     vips::VImage noise = vips::VImage::gaussnoise(image.width(), image.height(),
        vips::VImage::option()->set("sigma", sigma)->set("mean", 0.0));
     return image + noise;
}

// Apply unsharp mask sharpen with the given sigma.
vips::VImage morph_filters_apply_sharpen(vips::VImage image, double sigma) {
    return image.sharpen(vips::VImage::option()->set("sigma", sigma));
}

// Composite a watermark onto the image using gravity and opacity from options.
vips::VImage morph_filters_apply_watermark(vips::VImage image, MorphOptions *options) {
    if (options->watermark_path.empty()) return image;
    
    vips::VImage watermark;
    try {
        watermark = vips::VImage::new_from_file(options->watermark_path.c_str());
    } catch (...) {
        return image;
    }
    
    if (options->watermark_opacity < 1.0 && options->watermark_opacity >= 0.0) {
        if (watermark.has_alpha()) {
             vips::VImage alpha = watermark[3] * options->watermark_opacity;
             vips::VImage rgb = watermark.extract_band(0, vips::VImage::option()->set("n", 3));
             watermark = rgb.bandjoin(alpha);
        } else {
             vips::VImage alpha = vips::VImage::black(watermark.width(), watermark.height()) + (255 * options->watermark_opacity);
             watermark = watermark.bandjoin(alpha);
        }
    }
    
    int main_w = image.width();
    int main_h = image.height(); 
    if (image.get_typeof("page-height") != 0) {
        main_h = image.get_int("page-height");
    }
    
    int wm_w = watermark.width();
    int wm_h = watermark.height();
    
    int x = 0, y = 0;
    
    switch(options->watermark_gravity) {
        case MORPH_GRAVITY_CENTER: x = (main_w - wm_w)/2; y = (main_h - wm_h)/2; break;
        case MORPH_GRAVITY_TOP: x = (main_w - wm_w)/2; y = 0; break;
        case MORPH_GRAVITY_BOTTOM: x = (main_w - wm_w)/2; y = main_h - wm_h; break;
        case MORPH_GRAVITY_LEFT: x = 0; y = (main_h - wm_h)/2; break;
        case MORPH_GRAVITY_RIGHT: x = main_w - wm_w; y = (main_h - wm_h)/2; break;
        case MORPH_GRAVITY_TOP_LEFT: x = 0; y = 0; break;
        case MORPH_GRAVITY_TOP_RIGHT: x = main_w - wm_w; y = 0; break;
        case MORPH_GRAVITY_BOTTOM_LEFT: x = 0; y = main_h - wm_h; break;
        case MORPH_GRAVITY_BOTTOM_RIGHT: x = main_w - wm_w; y = main_h - wm_h; break;
    }
    
    x += options->watermark_x_offset;
    y += options->watermark_y_offset;
    
    return image.composite(watermark, VIPS_BLEND_MODE_OVER, vips::VImage::option()->set("x", x)->set("y", y));
}
