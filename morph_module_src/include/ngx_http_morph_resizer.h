#ifndef NGX_HTTP_MORPH_RESIZER_H
#define NGX_HTTP_MORPH_RESIZER_H

#include "std.h"

/**
 * morph_resizer_resize
 * @description Resize the image to specified dimensions. / 이미지를 지정된 크기로 리사이즈합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {int} width - Target width. / 목표 너비.
 * @param {int} height - Target height. / 목표 높이.
 * @returns {vips::VImage} - Resized image. / 리사이즈된 이미지.
 */
vips::VImage morph_resizer_resize(vips::VImage image, int width, int height);

/**
 * morph_resizer_crop
 * @description Crop the image to specified area. / 이미지를 지정된 영역으로 자릅니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {int} cx - Crop x position. / 자르기 시작 x 좌표.
 * @param {int} cy - Crop y position. / 자르기 시작 y 좌표.
 * @param {int} cw - Crop width. / 자르기 너비.
 * @param {int} ch - Crop height. / 자르기 높이.
 * @returns {vips::VImage} - Cropped image. / 잘린 이미지.
 */
vips::VImage morph_resizer_crop(vips::VImage image, int cx, int cy, int cw, int ch);

/**
 * morph_resizer_rotate
 * @description Rotate the image. / 이미지를 회전시킵니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} angle - Rotation angle. / 회전 각도.
 * @returns {vips::VImage} - Rotated image. / 회전된 이미지.
 */
vips::VImage morph_resizer_rotate(vips::VImage image, double angle);

/**
 * morph_resizer_flip
 * @description Flip the image horizontally or vertically. / 이미지를 수직 또는 수평으로 반전시킵니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {int} direction - 0 for vertical, 1 for horizontal. / 0은 수직, 1은 수평.
 * @returns {vips::VImage} - Flipped image. / 반전된 이미지.
 */
vips::VImage morph_resizer_flip(vips::VImage image, int direction);

#endif