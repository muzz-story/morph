#include "ngx_http_morph_filters.h"

/**
 * morph_filters_apply_blur
 * @description Apply gaussian blur to the image. / 이미지에 가우시안 블러를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} sigma - Blur sigma value. / 블러 강도(sigma).
 * @returns {vips::VImage} - Blurred image. / 블러 처리된 이미지.
 */
/**
 * morph_filters_apply_blur
 * @description Apply gaussian blur to the image. / 이미지에 가우시안 블러를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} sigma - Blur sigma value. / 블러 강도(sigma).
 * @returns {vips::VImage} - Blurred image. / 블러 처리된 이미지.
 */
vips::VImage morph_filters_apply_blur(vips::VImage image, double sigma)
{
    if (sigma <= 0.0) return image;
    return image.gaussblur(sigma);
}

/**
 * morph_filters_to_grayscale
 * @description Convert image to grayscale. / 이미지를 흑백으로 변환합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @returns {vips::VImage} - Grayscale image. / 흑백 변환된 이미지.
 */
vips::VImage morph_filters_to_grayscale(vips::VImage image)
{
    return image.colourspace(VIPS_INTERPRETATION_B_W);
}

/**
 * morph_filters_set_background
 * @description Flatten image with background color (for transparent images). / 배경색을 지정하여 이미지를 병합합니다(투명 이미지용).
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {const char*} color_hex - Hex color code (e.g., "FF00FF"). / 16진수 색상 코드.
 * @returns {vips::VImage} - Flattened image. / 배경색이 적용된 이미지.
 */
vips::VImage morph_filters_set_background(vips::VImage image, const char *color_hex)
{
    if (color_hex == NULL || strlen(color_hex) < 6) return image;

    // Remove # if present
    std::string hex_str(color_hex);
    if (hex_str[0] == '#') hex_str = hex_str.substr(1);

    int r, g, b;
    if (sscanf(hex_str.c_str(), "%02x%02x%02x", &r, &g, &b) != 3) {
         return image;
    }

    std::vector<double> bg = {(double)r, (double)g, (double)b};
    // If output is to be B/W, we might need 1 value, but flatten usually handles RGB.
    
    // vips flatten uses 'background' option which is VipsArrayDouble
    return image.flatten(vips::VImage::option()->set("background", bg));
}

/**
 * morph_filters_apply_brightness
 * @description Adjust image brightness. / 이미지 밝기를 조절합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} brightness - Brightness level (e.g., 1.0 for original). / 밝기 레벨 (예: 1.0이 원본).
 * @returns {vips::VImage} - Brightness adjusted image. / 밝기가 조절된 이미지.
 */
vips::VImage morph_filters_apply_brightness(vips::VImage image, double brightness)
{
    if (brightness == 1.0) return image;
    // Implemented as linear gain (Exposure-like)
    return image.linear(brightness, 0.0);
}

/**
 * morph_filters_apply_contrast
 * @description Adjust image contrast. / 이미지 대비를 조절합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} contrast - Contrast level (e.g., 1.0 for original). / 대비 레벨 (예: 1.0이 원본).
 * @returns {vips::VImage} - Contrast adjusted image. / 대비가 조절된 이미지.
 */
vips::VImage morph_filters_apply_contrast(vips::VImage image, double contrast)
{
    if (contrast == 1.0) return image;
    // Implemented as CSS-like contrast: (val - 128) * contrast + 128
    // Assuming 8-bit range mainly. 
    // formula: val * contrast + (128 - 128 * contrast)
    // For many VIPS operations, it promotes to float. 
    // Use 127.5 for midpoint? or 128? 128 is indicated in W3C filters.
    // Let's use 128.
    return image.linear(contrast, 128.0 * (1.0 - contrast));
}

/**
 * morph_filters_apply_noise
 * @description Apply noise to the image. / 이미지에 노이즈를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {int} type - Noise type. / 노이즈 타입.
 * @param {double} sigma - Noise sigma. / 노이즈 강도(sigma).
 * @returns {vips::VImage} - Noised image. / 노이즈가 적용된 이미지.
 */
vips::VImage morph_filters_apply_noise(vips::VImage image, int type, double sigma)
{
    if (sigma <= 0.0) return image;
    // ignoring type for now, default to gaussnoise
    // gaussnoise is a static generator in Vips C++ API
    // VImage gaussnoise(int width, int height, VOption *options = nullptr)
    vips::VImage noise = vips::VImage::gaussnoise(image.width(), image.height(), 
                                                  vips::VImage::option()->set("sigma", sigma)->set("mean", 0.0));
    
    // Convert noise to match image bands if needed, or Vips handles it?
    // Usually gaussnoise produces 1 band float. Image might be 3 bands uchar.
    // We add noise to image. Vips auto-expands 1 band to match mult-band image in arithmetic.
    
    // Note: The result of uchar + float will be float. We might need to cast back to original format?
    // But for a processing pipeline, keeping it float and casting later or letting Vips handle save is also fine.
    // However, to keep it simple and safe for standard pipeline expectation (often expects like-format input/output),
    // let's just return the addition. Vips will handle the format promotion.
    
    return image + noise;
}
