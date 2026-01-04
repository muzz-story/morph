
/**
 * morph_resizer_resize_smart
 * @description Smart Resize: Scale to cover, then crop based on gravity. / 스마트 리사이즈: 꽉 채우게 확대한 후 정렬 기준에 맞춰 자릅니다.
 * @param {vips::VImage} image - Input image.
 * @param {int} width - Target width.
 * @param {int} height - Target height.
 * @param {int} gravity - Gravity (0:Center, 1:Top, 2:Bottom, 3:Left, 4:Right).
 * @returns {vips::VImage} - Processed image.
 */
vips::VImage morph_resizer_resize_smart(vips::VImage image, int width, int height, int gravity)
{
    if (width <= 0 || height <= 0) return image;

    int input_w = image.width();
    int input_h = image.height();

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
        case MORPH_GRAVITY_TOP:
            cy = 0;
            break;
        case MORPH_GRAVITY_BOTTOM:
            cy = new_h - height;
            break;
        case MORPH_GRAVITY_LEFT:
            cx = 0;
            break;
        case MORPH_GRAVITY_RIGHT:
            cx = new_w - width;
            break;
        case MORPH_GRAVITY_CENTER:
        default:
            // Already centered
            break;
    }
    
    // Safety Bounds
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;

    // If result is somehow smaller than target (rounding errors?), stick to new_w/h
    int crop_w = (cx + width <= new_w) ? width : (new_w - cx);
    int crop_h = (cy + height <= new_h) ? height : (new_h - cy);
    
    return resized.extract_area(cx, cy, crop_w, crop_h);
}
