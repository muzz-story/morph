#ifndef NGX_HTTP_MORPH_FILTERS_H
#define NGX_HTTP_MORPH_FILTERS_H

#include "std.h"

/**
 * morph_filters_apply_blur
 * @description Apply gaussian blur to the image. / 이미지에 가우시안 블러를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} sigma - Blur sigma value. / 블러 강도(sigma).
 * @returns {vips::VImage} - Blurred image. / 블러 처리된 이미지.
 */
vips::VImage morph_filters_apply_blur(vips::VImage image, double sigma);

/**
 * morph_filters_to_grayscale
 * @description Convert image to grayscale. / 이미지를 흑백으로 변환합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @returns {vips::VImage} - Grayscale image. / 흑백 변환된 이미지.
 */
vips::VImage morph_filters_to_grayscale(vips::VImage image);

/**
 * morph_filters_set_background
 * @description Flatten image with background color (for transparent images). / 배경색을 지정하여 이미지를 병합합니다(투명 이미지용).
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {const char*} color_hex - Hex color code (e.g., "FF00FF"). / 16진수 색상 코드.
 * @returns {vips::VImage} - Flattened image. / 배경색이 적용된 이미지.
 */
vips::VImage morph_filters_set_background(vips::VImage image, const char *color_hex);

/**
 * morph_filters_apply_brightness
 * @description Adjust image brightness. / 이미지 밝기를 조절합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} brightness - Brightness level (e.g., 1.0 for original). / 밝기 레벨 (예: 1.0이 원본).
 * @returns {vips::VImage} - Brightness adjusted image. / 밝기가 조절된 이미지.
 */
vips::VImage morph_filters_apply_brightness(vips::VImage image, double brightness);

/**
 * morph_filters_apply_contrast
 * @description Adjust image contrast. / 이미지 대비를 조절합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} contrast - Contrast level (e.g., 1.0 for original). / 대비 레벨 (예: 1.0이 원본).
 * @returns {vips::VImage} - Contrast adjusted image. / 대비가 조절된 이미지.
 */
vips::VImage morph_filters_apply_contrast(vips::VImage image, double contrast);

/**
 * morph_filters_apply_noise
 * @description Apply noise to the image. / 이미지에 노이즈를 적용합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {int} type - Noise type. / 노이즈 타입.
 * @param {double} sigma - Noise sigma. / 노이즈 강도(sigma).
 * @returns {vips::VImage} - Noised image. / 노이즈가 적용된 이미지.
 */
vips::VImage morph_filters_apply_noise(vips::VImage image, int type, double sigma);

#endif