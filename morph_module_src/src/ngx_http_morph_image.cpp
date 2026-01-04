#include "std.h"
#include "ngx_http_morph_image.h"
#include "ngx_http_morph_reader.h"
#include "ngx_http_morph_resizer.h"
#include "ngx_http_morph_filters.h"
#include "ngx_http_morph_utils.h"
#include "ngx_http_morph_globals.h"

using namespace MorphUtils;

std::string morph_image_get_cache_path(MorphOptions* options) {
    std::string path = options->document_root;
    if (path.back() != '/') path += "/";
    
    path += options->service_name + "/";
    std::string safe_opts = sanitize_path(options->raw_options);
    path += safe_opts + "/";
    
    if (options->source_path.find("http") == 0) {
        std::string hash = compute_md5(options->source_path);        
        path += hash; 

        if (options->format == "png") path += ".png";
        else if (options->format == "webp") path += ".webp";
        else if (options->format == "gif") path += ".gif";
        else path += ".jpg";

    } else {
        path += options->source_path;
    }
    
    return path;
}

ngx_int_t morph_image_process(MorphOptions *options, std::string *out_data, ngx_log_t *log, bool *is_cache_hit, time_t *last_modified)
{
    if (is_cache_hit) *is_cache_hit = false;
    if (last_modified) *last_modified = 0;
    // 0. Cache Check
    std::string cache_path = morph_image_get_cache_path(options);
    bool cache_hit = false;
    
    if (access(cache_path.c_str(), F_OK) == 0) {
        // Prepare to check TTL
        struct stat st;
        if (stat(cache_path.c_str(), &st) == 0) {
            // Find service config
            int ttl = -1;
            if (g_morph_services.find(options->service_name) != g_morph_services.end()) {
                ttl = g_morph_services[options->service_name].ttl;
            }
            
            time_t now = time(NULL);
            if (ttl != -1 && (now - st.st_mtime) > ttl) {
                // 만료됨 (Expired)
                if (options->debug) {
                    MorphLogger::instance().debug("Cache EXPIRED (Age: %ds, TTL: %ds): %s", (int)(now - st.st_mtime), ttl, cache_path.c_str());
                }
                unlink(cache_path.c_str());
                // 캐시 미스로 처리 (Fallthrough to miss)
            } else {
                // 유효한 히트 (Valid Hit)
                cache_hit = true;
                if (last_modified) *last_modified = st.st_mtime;
            }
        }
    }
    
    if (cache_hit) {
        // 캐시 히트 (Cache Hit)
        if (options->debug) {
            MorphLogger::instance().debug("Cache HIT: %s", cache_path.c_str());
        }
        
        std::ifstream file(cache_path, std::ios::binary);
        if (file) {
            out_data->assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (is_cache_hit) *is_cache_hit = true;
            return NGX_OK;
        }
    } else {
        if (options->debug) {
            MorphLogger::instance().debug("Cache MISS: %s", cache_path.c_str());
        }
    }

    // 1. 이미지 가져오기 (Fetch Image)
    std::string image_data;
    if (morph_reader_read_source(options, &image_data, log) != NGX_OK) {
        return NGX_HTTP_NOT_FOUND;
    }

    // 2. Hex 검증 및 포맷 식별 (Validate Hex)
    int image_type = MORPH_IMG_UNKNOWN;
    if (morph_image_validate_hex((void*)image_data.data(), image_data.size(), &image_type) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Invalid image format");
        return NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
    }

    // 식별된 타입 로그 (Log the identified type)
    if (options->debug) {
        MorphLogger::instance().debug("Image Type Identified: %d (1:JPEG, 2:PNG, 3:WEBP, 4:GIF)", image_type);
    }

    // 3. VImage 생성 (로드 및 최적화)
    vips::VImage image;
    
    if (options->debug) {
        MorphLogger::instance().debug("Loading Image...");
    }
    
    try {
        bool use_thumbnail = false;

        if ((options->width > 0 || options->height > 0) && !options->has_crop) {
             use_thumbnail = true;
        }

        if (use_thumbnail) {
            vips::VOption *thumb_opts = vips::VImage::option()->set("no_rotate", false);
            if (options->height > 0) {
                thumb_opts->set("height", options->height);
            }         

            int load_width = options->width > 0 ? options->width : 10000;             
            image = vips::VImage::thumbnail_buffer(
                (void*)image_data.data(), 
                image_data.size(), 
                load_width, 
                thumb_opts
            );
            
            // Mark resize as done
            options->width = 0;
            options->height = 0; 
        } else {
            // Fallback: Simple Load
            image = vips::VImage::new_from_buffer(image_data.data(), image_data.size(), "");
        }
        
        // 4. Resize & Crop (Resizer)        
        // Apply Resize / Smart Crop
        if (options->width > 0 || options->height > 0) {
            if (options->has_crop) {
                // Manual Crop Case: Resize then Manual Crop?
                // Standard behavior: Resize to target, then manual crop relative to that?
                // Or: Manual crop first, then resize?
                // Current legacy logic: "Resize" usually means scale.
                // Let's keep original simple resize if manual crop is present.
                 if (options->debug) {
                    MorphLogger::instance().debug("Applying Simple Resize (Manual Crop waiting): %dx%d", options->width, options->height);
                }
                image = morph_resizer_resize(image, options->width, options->height);
            } else {
                // Smart Crop (Gravity)
                if (options->debug) {
                    MorphLogger::instance().debug("Applying Smart Resize (Gravity %d): %dx%d", options->gravity, options->width, options->height);
                }
                image = morph_resizer_resize_smart(image, options->width, options->height, options->gravity);
            }
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
            format_ext = ".gif"; 
        } else {
            format_ext = ".jpg";
            save_opts->set("Q", options->quality);
        }

        char *buf = NULL;
        size_t len = 0;
        
        if (options->debug) {
            MorphLogger::instance().debug("Output Location: Memory Buffer. Format: %s", format_ext.c_str());
        }

        image.write_to_buffer(format_ext.c_str(), (void**)&buf, &len, save_opts);
        
        if (buf && len > 0) {
            out_data->assign(buf, len);
            
            // 캐시에 저장 (Save to Cache)
            if (ensure_directory(cache_path) == 0) {
                std::ofstream outfile(cache_path, std::ios::binary);
                if (outfile) {
                    outfile.write(buf, len);
                    outfile.close();
                    if (options->debug) {
                        MorphLogger::instance().debug("Cache Saved: %s", cache_path.c_str());
                    }
                    if (last_modified) *last_modified = time(NULL);
                } else {
                     if (options->debug) {
                         MorphLogger::instance().debug("Cache Write Failed: %s", cache_path.c_str());
                    }
                }
            } else {
                 if (options->debug) {
                    MorphLogger::instance().debug("Cache Mkdir Failed: %s", cache_path.c_str());
                }
            }

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