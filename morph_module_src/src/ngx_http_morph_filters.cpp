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
#include "ngx_http_morph_resizer.h"

// Apply gaussian blur with the given sigma.
// 지정한 sigma로 가우시안 블러를 적용한다.
vips::VImage morph_filters_apply_blur(vips::VImage image, double sigma) {
    if (sigma <= 0.0) return image;
    return image.gaussblur(sigma);
}

// Convert image to grayscale.
// 이미지를 흑백으로 변환한다.
vips::VImage morph_filters_to_grayscale(vips::VImage image) {
    return image.colourspace(VIPS_INTERPRETATION_B_W);
}

// Flatten transparent image against the given hex background color (e.g., "FF00FF").
// 투명 영역을 지정한 16진 배경색(예: "FF00FF")으로 채워 평탄화한다.
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
    
    // flatten treats the last band as alpha whatever it is, so opaque images pass through untouched
    // flatten은 마지막 밴드를 무조건 알파로 보므로, 불투명 이미지는 그대로 통과시킨다
    if (!image.has_alpha()) return image;

    double scale = vips_interpretation_max_alpha(image.interpretation()) / 255.0;  // 16-bit: 0..65535 / 16비트는 0..65535
    if (image.bands() - 1 == 1) {
        bg_color = {0.2126 * bg_color[0] + 0.7152 * bg_color[1] + 0.0722 * bg_color[2]};  // grey + alpha / 회색 + 알파
    }
    for (double &v : bg_color) v *= scale;

    return image.flatten(vips::VImage::option()->set("background", bg_color));
}

// Apply fn to colour bands only and keep the original band format, so alpha is untouched
// and float results from linear()/noise are clipped back (gifsave rejects float input).
// 색 밴드에만 fn을 적용하고 원래 밴드 포맷을 유지한다. 알파는 건드리지 않고,
// linear()/노이즈의 float 결과도 원래 포맷으로 잘라 넣는다 (gifsave는 float 입력을 거부한다).
static vips::VImage morph_filters_on_colour(vips::VImage image, const std::function<vips::VImage(vips::VImage)>& fn) {
    VipsBandFormat fmt = image.format();
    if (!image.has_alpha()) return fn(image).cast(fmt);

    int n = image.bands() - 1;
    vips::VImage colour = image.extract_band(0, vips::VImage::option()->set("n", n));
    return fn(colour).cast(fmt).bandjoin(image.extract_band(n));
}

// Adjust brightness (1.0 = original).
// 밝기를 조절한다 (1.0 = 원본).
vips::VImage morph_filters_apply_brightness(vips::VImage image, double brightness) {
    return morph_filters_on_colour(image, [&](vips::VImage c) { return c.linear({brightness}, {0}); });
}

// Adjust contrast (1.0 = original). Pivots around mid-grey (128 for 8-bit, 32768 for 16-bit).
// 대비를 조절한다 (1.0 = 원본). 중간 회색(8비트 128, 16비트 32768)을 기준으로 한다.
vips::VImage morph_filters_apply_contrast(vips::VImage image, double contrast) {
    double mid = (vips_interpretation_max_alpha(image.interpretation()) + 1.0) / 2.0;
    return morph_filters_on_colour(image, [&](vips::VImage c) { return c.linear({contrast}, {mid * (1.0 - contrast)}); });
}

// Apply gaussian noise with the given sigma.
// 지정한 sigma로 가우시안 노이즈를 추가한다.
vips::VImage morph_filters_apply_noise(vips::VImage image, double sigma) {
    return morph_filters_on_colour(image, [&](vips::VImage c) {
        return c + vips::VImage::gaussnoise(c.width(), c.height(),
            vips::VImage::option()->set("sigma", sigma)->set("mean", 0.0));
    });
}

// Apply unsharp mask sharpen with the given sigma.
// 지정한 sigma로 언샤프 마스크 샤프닝을 적용한다.
vips::VImage morph_filters_apply_sharpen(vips::VImage image, double sigma) {
    return image.sharpen(vips::VImage::option()->set("sigma", sigma));
}

// watermark_path is a bare file name (sanitized, no '/'), looked up in {root}/{service}/ like local sources.
// watermark_path는 정리된 파일명('/' 없음)이고, 로컬 원본처럼 {root}/{service}/에서 찾는다.
std::string morph_filters_watermark_file(MorphOptions *options) {
    std::string path = options->document_root;
    if (!path.empty() && path.back() != '/') path += "/";
    return path + options->service_name + "/" + options->watermark_path;
}

// Composite a watermark onto every frame using gravity and opacity from options.
// Both sides are brought to 8-bit sRGB first, so 16-bit or grey watermarks/bases blend correctly.
// options의 gravity와 opacity로 모든 프레임에 워터마크를 합성한다.
// 양쪽을 먼저 8비트 sRGB로 맞춰서 16비트나 흑백 워터마크/원본도 제대로 합성된다.
vips::VImage morph_filters_apply_watermark(vips::VImage image, MorphOptions *options) {
    if (options->watermark_path.empty()) return image;

    // load errors propagate as VError -> 500 (existence is checked before processing -> 404)
    // 로드 오류는 VError로 올라가 500이 된다 (파일이 있는지는 처리 전에 확인해 404)
    vips::VImage watermark = vips::VImage::new_from_file(morph_filters_watermark_file(options).c_str())
                                 .colourspace(VIPS_INTERPRETATION_sRGB);

    if (!watermark.has_alpha()) watermark = watermark.bandjoin_const({255});
    if (options->watermark_opacity < 1.0) {
        int n = watermark.bands() - 1;  // alpha is the last band (grey+A has 2, RGB+A has 4) / 알파는 마지막 밴드 (회색+A는 2개, RGB+A는 4개)
        vips::VImage alpha = (watermark.extract_band(n) * options->watermark_opacity).cast(watermark.format());
        watermark = watermark.extract_band(0, vips::VImage::option()->set("n", n)).bandjoin(alpha);
    }

    int wm_w = watermark.width();
    int wm_h = watermark.height();

    return morph_resizer_per_frame(image, [&](vips::VImage frame) {
        frame = frame.colourspace(VIPS_INTERPRETATION_sRGB);
        int main_w = frame.width();
        int main_h = frame.height();
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

        // composite always adds alpha; flatten it back off for opaque bases
        // composite는 항상 알파를 붙이므로, 불투명 원본이면 flatten으로 다시 뗀다
        vips::VImage out = frame.composite(watermark, VIPS_BLEND_MODE_OVER, vips::VImage::option()->set("x", x)->set("y", y));
        return frame.has_alpha() ? out.cast(VIPS_FORMAT_UCHAR) : out.flatten().cast(VIPS_FORMAT_UCHAR);
    });
}
