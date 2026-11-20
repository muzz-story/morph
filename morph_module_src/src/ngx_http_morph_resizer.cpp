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

// Run fn on every frame of an animated (page-height) image and rejoin, so geometry and
// position-dependent filters act per frame instead of on the whole frame strip.
// 애니메이션(page-height) 이미지의 프레임마다 fn을 실행하고 다시 이어 붙인다. 그래서 기하 변환과
// 위치에 따라 달라지는 필터가 프레임 띠 전체가 아니라 프레임별로 적용된다.
vips::VImage morph_resizer_per_frame(vips::VImage image, const std::function<vips::VImage(vips::VImage)>& fn)
{
    int page_height = image.get_typeof("page-height") != 0 ? image.get_int("page-height") : 0;
    if (page_height <= 0 || page_height >= image.height()) return fn(image);

    int n_pages = image.height() / page_height;
    std::vector<vips::VImage> frames;
    for (int i = 0; i < n_pages; ++i) {
        frames.push_back(fn(image.extract_area(0, i * page_height, image.width(), page_height)));
    }

    vips::VImage result = vips::VImage::arrayjoin(frames, vips::VImage::option()->set("across", 1));
    result.set("page-height", frames[0].height());
    if (image.get_typeof("delay") != 0) result.set("delay", image.get_array_int("delay"));
    if (image.get_typeof("loop") != 0) result.set("loop", image.get_int("loop"));
    return result;
}

// Resize keeping aspect ratio when one side is 0; stretches when both are given.
// 한쪽이 0이면 비율을 유지하며 리사이즈하고, 둘 다 주면 늘려서 맞춘다.
vips::VImage morph_resizer_resize(vips::VImage image, int width, int height)
{
    if (width <= 0 && height <= 0) return image;

    return morph_resizer_per_frame(image, [&](vips::VImage frame) {
        double hscale = width > 0 ? (double)width / frame.width() : (double)height / frame.height();
        double vscale = height > 0 ? (double)height / frame.height() : hscale;
        return frame.resize(hscale, vips::VImage::option()->set("vscale", vscale));
    });
}

// Scale one frame to cover width x height, then crop by gravity.
// 프레임 하나를 width x height가 꽉 차게 스케일한 뒤 gravity로 자른다.
static vips::VImage morph_resizer_cover_crop(vips::VImage frame, int width, int height, int gravity)
{
    double scale_x = (double)width / frame.width();
    double scale_y = (double)height / frame.height();
    vips::VImage resized = frame.resize(scale_x > scale_y ? scale_x : scale_y);

    int new_w = resized.width();
    int new_h = resized.height();
    int cx = (new_w - width) / 2;
    int cy = (new_h - height) / 2;

    switch (gravity) {
        case MORPH_GRAVITY_TOP: cy = 0; break;
        case MORPH_GRAVITY_BOTTOM: cy = new_h - height; break;
        case MORPH_GRAVITY_LEFT: cx = 0; break;
        case MORPH_GRAVITY_RIGHT: cx = new_w - width; break;
        case MORPH_GRAVITY_CENTER: default: break;
    }
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;

    int crop_w = (cx + width <= new_w) ? width : (new_w - cx);
    int crop_h = (cy + height <= new_h) ? height : (new_h - cy);
    return resized.extract_area(cx, cy, crop_w, crop_h);
}

// Smart resize: scale to cover, then crop based on gravity.
// 스마트 리사이즈: cover로 스케일한 뒤 gravity 기준으로 자른다.
vips::VImage morph_resizer_resize_smart(vips::VImage image, int width, int height, int gravity)
{
    if (width <= 0 || height <= 0) return image;

    int page_height = image.get_typeof("page-height") != 0 ? image.get_int("page-height") : 0;
    int frame_h = page_height > 0 ? page_height : image.height();

    // Already exact (thumbnail_buffer did the cover crop)
    // 이미 정확한 크기 (thumbnail_buffer가 cover 크롭을 했다)
    if (image.width() == width && frame_h == height) return image;

    return morph_resizer_per_frame(image, [&](vips::VImage frame) {
        return morph_resizer_cover_crop(frame, width, height, gravity);
    });
}

// Crop the image to the specified area (cx, cy, cw, ch), clamped to the frame.
// 지정한 영역(cx, cy, cw, ch)으로 자르되, 프레임 안으로 제한한다.
vips::VImage morph_resizer_crop(vips::VImage image, int cx, int cy, int cw, int ch)
{
    return morph_resizer_per_frame(image, [&](vips::VImage frame) {
        int img_w = frame.width();
        int img_h = frame.height();
        int x = cx < 0 ? 0 : (cx >= img_w ? img_w - 1 : cx);
        int y = cy < 0 ? 0 : (cy >= img_h ? img_h - 1 : cy);
        int w = (cw <= 0 || cw > img_w - x) ? img_w - x : cw;  // no x + cw: overflows near INT_MAX / x + cw를 쓰지 않음: INT_MAX 근처에서 오버플로
        int h = (ch <= 0 || ch > img_h - y) ? img_h - y : ch;
        return frame.extract_area(x, y, w, h);
    });
}

// Rotate the image by the given angle (degrees).
// 지정한 각도(도 단위)로 회전한다.
vips::VImage morph_resizer_rotate(vips::VImage image, double angle)
{
    angle = fmod(angle, 360.0);
    if (angle < 0) angle += 360.0;
    if (angle == 0.0) return image;

    return morph_resizer_per_frame(image, [&](vips::VImage frame) {
        if (angle == 90.0) return frame.rot(VIPS_ANGLE_D90);
        if (angle == 180.0) return frame.rot(VIPS_ANGLE_D180);
        if (angle == 270.0) return frame.rot(VIPS_ANGLE_D270);
        return frame.similarity(vips::VImage::option()->set("angle", angle));
    });
}

// Flip the image: direction 0 = vertical, 1 = horizontal.
// 이미지를 뒤집는다: 0 = 상하, 1 = 좌우.
vips::VImage morph_resizer_flip(vips::VImage image, int direction)
{
    return morph_resizer_per_frame(image, [&](vips::VImage frame) {
        return frame.flip(direction == 1 ? VIPS_DIRECTION_HORIZONTAL : VIPS_DIRECTION_VERTICAL);
    });
}
