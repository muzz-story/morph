#include "std.h"
#include "ngx_http_morph_filters.h"

/**
 * morph_filters_apply_blur
 * @description Apply gaussian blur to the image. / 이미지에 가우시안 블러를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} sigma - Blur sigma value. / 블러 강도(sigma).
 * @returns {vips::VImage} - Blurred image. / 블러 처리된 이미지.
 */
vips::VImage morph_filters_apply_blur(vips::VImage image, double sigma) {
    if (sigma <= 0.0) return image;
    return image.gaussblur(sigma);
}

/**
 * morph_filters_to_grayscale
 * @description Convert image to grayscale. / 이미지를 흑백으로 변환합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @returns {vips::VImage} - Grayscale image. / 흑백 변환된 이미지.
 */
vips::VImage morph_filters_to_grayscale(vips::VImage image) {
    return image.colourspace(VIPS_INTERPRETATION_B_W);
}

/**
 * morph_filters_set_background
 * @description Flatten image with background color (for transparent images). / 배경색을 지정하여 이미지를 병합합니다(투명 이미지용).
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {const char*} color_hex - Hex color code (e.g., "FF00FF"). / 16진수 색상 코드.
 * @returns {vips::VImage} - Flattened image. / 배경색이 적용된 이미지.
 */
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

/**
 * morph_filters_apply_brightness
 * @description Adjust image brightness. / 이미지 밝기를 조절합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} brightness - Brightness level (e.g., 1.0 for original). / 밝기 레벨 (예: 1.0이 원본).
 * @returns {vips::VImage} - Brightness adjusted image. / 밝기가 조절된 이미지.
 */
vips::VImage morph_filters_apply_brightness(vips::VImage image, double brightness) {
    return image.linear({brightness}, {0});
}

/**
 * morph_filters_apply_contrast
 * @description Adjust image contrast. / 이미지 대비를 조절합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} contrast - Contrast level (e.g., 1.0 for original). / 대비 레벨 (예: 1.0이 원본).
 * @returns {vips::VImage} - Contrast adjusted image. / 대비가 조절된 이미지.
 */
vips::VImage morph_filters_apply_contrast(vips::VImage image, double contrast) {
    return image.linear({contrast}, {0});
}

/**
 * morph_filters_apply_noise
 * @description Apply noise to the image. / 이미지에 노이즈를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {int} type - Noise type. / 노이즈 타입.
 * @param {double} sigma - Noise sigma. / 노이즈 강도(sigma).
 * @returns {vips::VImage} - Noised image. / 노이즈가 적용된 이미지.
 */
vips::VImage morph_filters_apply_noise(vips::VImage image, int type, double sigma) {
     return image.gaussnoise(sigma);
}

/**
 * morph_filters_apply_sharpen
 * @description Apply sharpen filter. / 이미지에 선명 효과(Sharpen)를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} sigma - Sharpen sigma value. / 선명도 강도(sigma).
 * @returns {vips::VImage} - Sharpened image. / 선명도가 조절된 이미지.
 */
vips::VImage morph_filters_apply_sharpen(vips::VImage image, double sigma) {
    return image.sharpen(vips::VImage::option()->set("sigma", sigma));
}

/**
 * morph_filters_apply_watermark
 * @description Apply watermark image. / 이미지에 워터마크를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {MorphOptions*} options - Image processing options containing watermark settings. / 워터마크 설정을 포함한 이미지 처리 옵션.
 * @returns {vips::VImage} - Image with watermark. / 워터마크가 적용된 이미지.
 */
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
            // Add alpha if none
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
