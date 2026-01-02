#include "ngx_http_morph_resizer.h"

/**
 * morph_resizer_resize
 * @description Resize the image to specified dimensions. / 이미지를 지정된 크기로 리사이즈합니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {int} width - Target width. / 목표 너비.
 * @param {int} height - Target height. / 목표 높이.
 * @returns {vips::VImage} - Resized image. / 리사이즈된 이미지.
 */
vips::VImage morph_resizer_resize(vips::VImage image, int width, int height)
{
    if (width <= 0 && height <= 0) return image;

    double scale = 1.0;
    double vscale = 1.0;

    int input_w = image.width();
    int input_h = image.height();

    if (width > 0 && height > 0) {
        // Distort/Stretch to fit? Or Fit within?
        // Let's assume standard behavior: strict resize (distort) if both are forced,
        // unless we want to keep aspect ratio.
        // For now, let's calculate scales independently to match target exactly.
        scale = (double)width / input_w;
        vscale = (double)height / input_h;
        
        return image.resize(scale, vips::VImage::option()->set("vscale", vscale));
    } else if (width > 0) {
        scale = (double)width / input_w;
        return image.resize(scale);
    } else {
        scale = (double)height / input_h;
        return image.resize(scale);
    }
}

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
vips::VImage morph_resizer_crop(vips::VImage image, int cx, int cy, int cw, int ch)
{
    int img_w = image.width();
    int img_h = image.height();

    // Boundary Validation
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cw <= 0) cw = img_w;
    if (ch <= 0) ch = img_h;
    if (cx + cw > img_w) cw = img_w - cx;
    if (cy + ch > img_h) ch = img_h - cy;

    return image.extract_area(cx, cy, cw, ch);
}

/**
 * morph_resizer_rotate
 * @description Rotate the image. / 이미지를 회전시킵니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {double} angle - Rotation angle. / 회전 각도.
 * @returns {vips::VImage} - Rotated image. / 회전된 이미지.
 */
vips::VImage morph_resizer_rotate(vips::VImage image, double angle)
{
    // Normalize angle to 0-360
    while(angle < 0) angle += 360;
    while(angle >= 360) angle -= 360;

    if (angle == 0.0) return image;
    
    // Check for 90 degree increments
    if (angle == 90.0) return image.rot(VIPS_ANGLE_D90);
    if (angle == 180.0) return image.rot(VIPS_ANGLE_D180);
    if (angle == 270.0) return image.rot(VIPS_ANGLE_D270);

    // Arbitrary rotation
    // Note: similarity rotates about 0,0 (top left). We might want center rotation.
    // But basic similarity usage:
    return image.similarity(vips::VImage::option()->set("angle", angle));
}

/**
 * morph_resizer_flip
 * @description Flip the image horizontally or vertically. / 이미지를 수직 또는 수평으로 반전시킵니다.
 * @param {vips::VImage} image - Input image. / 입력 이미지.
 * @param {int} direction - 0 for vertical, 1 for horizontal. / 0은 수직, 1은 수평.
 * @returns {vips::VImage} - Flipped image. / 반전된 이미지.
 */
vips::VImage morph_resizer_flip(vips::VImage image, int direction)
{
    if (direction == 1) {
        return image.flip(VIPS_DIRECTION_HORIZONTAL);
    } else {
        return image.flip(VIPS_DIRECTION_VERTICAL);
    }
}