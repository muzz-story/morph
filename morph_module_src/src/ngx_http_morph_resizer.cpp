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
#include "ngx_http_morph_resizer.h"
#include "ngx_http_morph_types.h"

vips::VImage morph_resizer_resize(vips::VImage image, int width, int height);

// Helper: Process animated GIF frames one by one for smart resize.
static vips::VImage morph_resizer_process_animated_frames(vips::VImage image, int width, int height, int input_w, int input_h, int page_height, int gravity, int calc_h) {
    int n_pages = input_h / page_height;
    std::vector<vips::VImage> frames;
    
    // 1. Calculate Scale to COVER (same for all frames)
    double scale_x = (double)width / input_w;
    double scale_y = (double)height / calc_h;
    double scale = (scale_x > scale_y) ? scale_x : scale_y;
    
    // 2. Loop and process
    for (int i = 0; i < n_pages; ++i) {
        // Extract Frame
        vips::VImage frame = image.extract_area(0, i * page_height, input_w, page_height);
        
        // Resize Frame
        vips::VImage r_frame = frame.resize(scale);
        
        // Calculate Crop (on first frame only optimization possible, but fast enough)
        int r_w = r_frame.width();
        int r_h = r_frame.height();
        int cx = 0, cy = 0;
        
        // Default Center
        cx = (r_w - width) / 2;
        cy = (r_h - height) / 2;

        switch (gravity) {
            case MORPH_GRAVITY_TOP: cy = 0; break;
            case MORPH_GRAVITY_BOTTOM: cy = r_h - height; break;
            case MORPH_GRAVITY_LEFT: cx = 0; break;
            case MORPH_GRAVITY_RIGHT: cx = r_w - width; break;
            case MORPH_GRAVITY_CENTER: default: break;
        }
        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        
        // Ensure crop doesn't exceed bounds
        int cw = (cx + width <= r_w) ? width : (r_w - cx);
        int ch = (cy + height <= r_h) ? height : (r_h - cy);
        
        // Crop Frame
        frames.push_back(r_frame.extract_area(cx, cy, cw, ch));
    }
    
    // 3. Rejoin Frames
    vips::VImage result = vips::VImage::arrayjoin(frames, vips::VImage::option()->set("across", 1));
    
    int final_page_height = frames[0].height();
    result.set("page-height", final_page_height);

    // Copy optional animation metadata (delay, loop) if present
    if (image.get_typeof("delay") != 0) {
        result.set("delay", image.get_array_int("delay"));
    }
    if (image.get_typeof("loop") != 0) {
        result.set("loop", image.get_int("loop"));
    }
    
    return result;
}

// Resize the image to specified dimensions.
vips::VImage morph_resizer_resize(vips::VImage image, int width, int height)
{
    if (width <= 0 && height <= 0) return image;

    double scale = 1.0;
    double vscale = 1.0;

    int input_w = image.width();
    int input_h = image.height();
    int page_height = 0;
    if (image.get_typeof("page-height") != 0) {
        page_height = image.get_int("page-height");
    }
    int calc_h = (page_height > 0) ? page_height : input_h;

    vips::VImage result;

    // Fixed Resize (Stretch/Squash if both W/H provided)
    if (width > 0 && height > 0) {
        scale = (double)width / input_w;
        vscale = (double)height / calc_h;
        result = image.resize(scale, vips::VImage::option()->set("vscale", vscale));
    } else if (width > 0) {
        scale = (double)width / input_w;
        result = image.resize(scale);
    } else {
        scale = (double)height / calc_h;
        result = image.resize(scale);
    }

    if (page_height > 0) {
        int new_page_height = (int)round(page_height * vscale);
        int n_pages = input_h / page_height;
        new_page_height = result.height() / n_pages;
        result.set("page-height", new_page_height);
    }
    return result;
}

// Helper: Process static image with cover-scale and gravity-based crop.
static vips::VImage morph_resizer_process_static_image(vips::VImage image, int width, int height, int input_w, int input_h, int gravity) {
    // 1. Calculate Scale to COVER (max of w_scale, h_scale)
    double scale_x = (double)width / input_w;
    double scale_y = (double)height / input_h;
    double scale = (scale_x > scale_y) ? scale_x : scale_y;

    // 2. Resize
    vips::VImage resized = image.resize(scale);
    
    // 3. Calculate Crop Coordinates from Resized Image
    int new_w = resized.width();
    int new_h = resized.height();
    
    int cx = 0, cy = 0;

    // Default Center
    cx = (new_w - width) / 2;
    cy = (new_h - height) / 2;

    switch (gravity) {
        case MORPH_GRAVITY_TOP: cy = 0; break;
        case MORPH_GRAVITY_BOTTOM: cy = new_h - height; break;
        case MORPH_GRAVITY_LEFT: cx = 0; break;
        case MORPH_GRAVITY_RIGHT: cx = new_w - width; break;
        case MORPH_GRAVITY_CENTER: default: break;
    }
    
    // Safety Bounds
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;

    int crop_w = (cx + width <= new_w) ? width : (new_w - cx);
    int crop_h = (cy + height <= new_h) ? height : (new_h - cy);
    
    return resized.extract_area(cx, cy, crop_w, crop_h);
}

// Smart resize: scale to cover, then crop based on gravity.
vips::VImage morph_resizer_resize_smart(vips::VImage image, int width, int height, int gravity)
{
    if (width <= 0 || height <= 0) return image;

    int input_w = image.width();
    int input_h = image.height();
    int page_height = 0;
    if (image.get_typeof("page-height") != 0) {
        page_height = image.get_int("page-height");
    }
    int calc_h = (page_height > 0) ? page_height : input_h;

    // Optimization: If already correct size (handled by thumbnail_buffer), return early
    if (input_w == width && calc_h == height) {
        return image;
    }

    // --- Animation Handling: Frame-by-Frame Processing ---
    if (page_height > 0) {
        return morph_resizer_process_animated_frames(image, width, height, input_w, input_h, page_height, gravity, calc_h);
    }

    // --- Static Image Handling (Original Logic) ---
    return morph_resizer_process_static_image(image, width, height, input_w, input_h, gravity);
}

// Crop the image to the specified area (cx, cy, cw, ch).
vips::VImage morph_resizer_crop(vips::VImage image, int cx, int cy, int cw, int ch)
{
    int img_w = image.width();
    int img_h = image.height();

    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cw <= 0) cw = img_w;
    if (ch <= 0) ch = img_h;
    if (cx + cw > img_w) cw = img_w - cx;
    if (cy + ch > img_h) ch = img_h - cy;

    return image.extract_area(cx, cy, cw, ch);
}

// Rotate the image by the given angle (degrees).
vips::VImage morph_resizer_rotate(vips::VImage image, double angle)
{
    while(angle < 0) angle += 360;
    while(angle >= 360) angle -= 360;

    if (angle == 0.0) return image;    
    if (angle == 90.0) return image.rot(VIPS_ANGLE_D90);
    if (angle == 180.0) return image.rot(VIPS_ANGLE_D180);
    if (angle == 270.0) return image.rot(VIPS_ANGLE_D270);

    return image.similarity(vips::VImage::option()->set("angle", angle));
}

// Flip the image: direction 0 = vertical, 1 = horizontal.
vips::VImage morph_resizer_flip(vips::VImage image, int direction)
{
    if (direction == 1) {
        return image.flip(VIPS_DIRECTION_HORIZONTAL);
    } else {
        return image.flip(VIPS_DIRECTION_VERTICAL);
    }
}