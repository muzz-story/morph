#include "ngx_http_morph_image.h"
#include "ngx_http_morph_reader.h"
#include "ngx_http_morph_resizer.h"
#include "ngx_http_morph_filters.h"

// Forward declaration of MorphOptions (used in void* cast)
// In real implementation, include the shared header defining options.

/**
 * morph_image_process
 * @description Orchestrate the entire image processing flow: Read -> Check Hex -> Resize -> Filter -> Output. / 전체 이미지 처리 흐름을 관리합니다: 읽기 -> Hex 확인 -> 리사이즈 -> 필터 -> 출력.
 * @param {MorphOptions*} options - MorphOptions structure pointer. / MorphOptions 구조체 포인터.
 * @param {std::string*} out_data - Output image data buffer. / 출력 이미지 데이터 버퍼.
 * @param {ngx_log_t*} log - Logger. / 로거.
 * @returns {ngx_int_t} - NGX_OK or error. / 성공 시 NGX_OK 또는 에러.
 */
ngx_int_t morph_image_process(MorphOptions *options, std::string *out_data, ngx_log_t *log)
{
    // 1. Fetch Image (Reader) / 이미지 가져오기
    std::string image_data;
    if (morph_reader_read_source(options, &image_data, log) != NGX_OK) {
        return NGX_HTTP_NOT_FOUND;
    }

    // 2. Validate Hex (Image) / Hex 검증 및 포맷 식별
    int image_type = MORPH_IMG_UNKNOWN;
    if (morph_image_validate_hex((void*)image_data.data(), image_data.size(), &image_type) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Invalid image format");
        return NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
    }

    // Log the identified type
    ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Identified Image Type: %d", image_type);

    // 3. Create VImage (Loading & Optimizing) / Vips 이미지 로드 및 최적화
    vips::VImage image;
    try {
        // Optimization: Use thumbnail_buffer for shrink-on-load
        if (options->width > 0 || options->height > 0) {
            // If Resize Needed -> Use thumbnail_buffer
            // Vips thumbnail automatically handles:
            // - Shrink-on-load (jpeg, webp, etc)
            // - Autorotate (usually)
            // - Linear colorspace processing
            
            // Construct option string
            // We need to pass buffer pointer and length
            // thumbnail_buffer(blob, width, "height", height, ...)
            
            // To pass raw buffer without copy, we use a Blob.
            // But C++ wrapper makes it easier: VImage::thumbnail_buffer(void* data, size_t len, int width, ...)
            
            vips::VOption *thumb_opts = vips::VImage::option()->set("no_rotate", false); // Autorotate by default
            
            if (options->height > 0) {
                thumb_opts->set("height", options->height);
                if (options->width <= 0) {
                     // Only height set -> aspect ratio preserve logic handled by thumbnail?
                     // Vips thumbnail requries width.
                     // If only height is known, we might need to load header first or let vips handle "size" logic.
                     // Vips thumbnail: "width" is required. "height" is optional.
                     // If we want height driven, we might need another approach or set width to large?
                     // Actually vips_thumbnail has "size" option (VIPS_SIZE_DOWN, etc).
                     
                     // For simplicity in this mock/stub env:
                     // If only height is set, we use simple load + resize because thumbnail needs width primarily.
                     // OR we can guess width.
                     // Let's stick to: Use thumbnail if width is present.
                     goto fallback_load; 
                }
            } else {
                 // Only width
            }
            
            // Handle Crop within thumbnail?
            // "crop" option in thumbnail: VIPS_INTERESTING_NONE (default), _CENTRE, _ENTROPY, _ATTENTION
            // If we have manual crop (cx,cy,cw,ch), we should NOT let thumbnail crop.
            // We want thumbnail to "fit" the target box, then we crop manually if needed?
            // Wait, manual crop comes AFTER resize usually? No, BEFORE resize.
            // Crop -> Resize.
            // If we use thumbnail, it does Resize.
            // So if Crop is needed, we cannot use thumbnail EASILY unless we crop the result (which is incorrect order).
            
            // Correct Order: Crop -> Resize.
            // Vips Thumbnail Order: Load -> Shrink -> Crop(Smart) -> Resize.
            
            // Conflict: We have manual Crop coordinates based on ORIGINAL image.
            // If we use thumbnail, we lose original coordinates mapping.
            
            // Conclusion: 
            // If has_crop is true -> Use standard Load (new_from_buffer) -> Crop -> Resize.
            // If has_crop is false -> Use thumbnail_buffer (Load + Resize).
            
            if (options->has_crop) {
                goto fallback_load;
            }

            // Safe to use thumbnail
            image = vips::VImage::thumbnail_buffer(
                (void*)image_data.data(), 
                image_data.size(), 
                options->width, 
                thumb_opts
            );
            
            // Since thumbnail already resized, we skip manual resize
            // We need to mark that resize is done to avoid double resize?
            // Let's set dimensions to 0 in options so subsequent resize block is skipped.
            options->width = 0;
            options->height = 0; 

        } else {
            // No Resize needed -> Simple Load
            fallback_load:
            image = vips::VImage::new_from_buffer(image_data.data(), image_data.size(), "");
        }
        
        // 4. Resize & Crop (Resizer)
        
        // 4. Resize & Crop (Resizer)
        // Crop first? Or Resize first?
        // Usually resize then crop if thumbnailing, but here we have explicit ops.
        // Let's follow: Crop -> Resize -> Rotate/Flip (Geometric) -> Filters
        // Or user constraints dependent.
        // Typically: 
        // 1. Resize (to approximate)
        // 2. Crop (extract area)
        // 3. Rotate
        // 4. Filters
        
        // Apply Resize
        if (options->width > 0 || options->height > 0) {
            image = morph_resizer_resize(image, options->width, options->height);
        }

        // Apply Crop
        if (options->has_crop) {
            image = morph_resizer_crop(image, options->cx, options->cy, options->cw, options->ch);
        }

        // Apply Rotate
        if (options->rotate_angle != 0.0) {
            image = morph_resizer_rotate(image, options->rotate_angle);
        }

        // Apply Flip
        if (options->flip) {
            image = morph_resizer_flip(image, options->flip_dir);
        }

        // 5. Transform (Filters)
        
        // Background Color (Flatten)
        if (!options->bg_color.empty()) {
            image = morph_filters_set_background(image, options->bg_color.c_str());
        }

        // Grayscale
        if (options->grayscale) {
            image = morph_filters_to_grayscale(image);
        }

        // Brightness
        if (options->brightness != 1.0) {
            image = morph_filters_apply_brightness(image, options->brightness);
        }

        // Contrast
        if (options->contrast != 1.0) {
            image = morph_filters_apply_contrast(image, options->contrast);
        }

        // Blur
        if (options->blur_sigma > 0.0) {
            image = morph_filters_apply_blur(image, options->blur_sigma);
        }

        // Noise
        if (options->noise_sigma > 0.0) {
             // 0 for default gaussion
            image = morph_filters_apply_noise(image, 0, options->noise_sigma);
        }
        
        // 6. Output
        std::string format_ext = ".jpg";
        vips::VOption *save_opts = vips::VImage::option();

        if (options->format == "png") {
            format_ext = ".png";
        } else if (options->format == "webp") {
            format_ext = ".webp";
            save_opts->set("Q", options->quality);
        } else if (options->format == "gif") {
            format_ext = ".gif"; // Magick save? Vips usually saves gif as animated or static.
        } else {
            // Default JPG
            format_ext = ".jpg";
            save_opts->set("Q", options->quality);
        }

        // Buffer write
        // Note: write_to_buffer returns void* buffer and size, wrapper writes to VBuf?
        // C++ wrapper: write_to_buffer(suffix, options) returns Blob
        // Actually: void write_to_buffer (const char *suffix, void **buf, size_t *size, VOption *options=0) const
        
        char *buf = NULL;
        size_t len = 0;
        
        image.write_to_buffer(format_ext.c_str(), (void**)&buf, &len, save_opts);
        
        if (buf && len > 0) {
            out_data->assign(buf, len);
            g_free(buf); // Vips uses GLib allocs for buffer return
        } else {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        
    } catch (vips::VError &e) {
         ngx_log_error(NGX_LOG_ERR, log, 0, "Vips Processing Error: %s", e.what());
         return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    return NGX_OK;
}

/**
 * morph_image_validate_hex
 * @description Validate file magic numbers and identify format. / 파일 매직 넘버를 검사하여 유효성을 확인하고 포맷을 식별합니다.
 * @param {void*} data - Pointer to file data. / 파일 데이터 포인터.
 * @param {size_t} len - Data length. / 데이터 길이.
 * @param {int*} out_type - Identified image type (MorphImageType). / 식별된 이미지 타입.
 * @returns {ngx_int_t} - NGX_OK (valid) or NGX_ERROR (invalid). / 유효하면 NGX_OK, 아니면 NGX_ERROR.
 */
ngx_int_t morph_image_validate_hex(void *data, size_t len, int *out_type)
{
    if (out_type) *out_type = MORPH_IMG_UNKNOWN;

    if (len < 12) { // Minimum length check
        return NGX_ERROR;
    }

    unsigned char *bytes = (unsigned char *)data;

    // JPEG: FF D8
    if (bytes[0] == 0xFF && bytes[1] == 0xD8) {
        if (out_type) *out_type = MORPH_IMG_JPEG;
        return NGX_OK;
    }

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47 &&
        bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A) {
        if (out_type) *out_type = MORPH_IMG_PNG;
        return NGX_OK;
    }

    // GIF: GIF87a or GIF89a (47 49 46 38 39 61)
    if (bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' &&
        bytes[3] == '8' && (bytes[4] == '7' || bytes[4] == '9') && bytes[5] == 'a') {
        if (out_type) *out_type = MORPH_IMG_GIF;
        return NGX_OK;
    }

    // WEBP: RIFF .... WEBP
    // 0-3: RIFF
    // 8-11: WEBP
    if (bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
        bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P') {
        if (out_type) *out_type = MORPH_IMG_WEBP;
        return NGX_OK;
    }

    return NGX_ERROR;
}