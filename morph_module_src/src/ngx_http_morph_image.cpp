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

// Helper: Check Cache
static bool morph_check_cache(MorphOptions *options, std::string *out_data, time_t *last_modified) {
    std::string cache_path = morph_image_get_cache_path(options);
    if (access(cache_path.c_str(), F_OK) != 0) return false;

    struct stat st;
    if (stat(cache_path.c_str(), &st) != 0) return false;

    // Check TTL
    if (g_morph_services.find(options->service_name) != g_morph_services.end()) {
        int ttl = g_morph_services[options->service_name].ttl;
        if (ttl != -1 && (time(NULL) - st.st_mtime) > ttl) {
            unlink(cache_path.c_str());
            return false;
        }
    }

    // Hit
    std::ifstream file(cache_path, std::ios::binary);
    if (file) {
        out_data->assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (last_modified) *last_modified = st.st_mtime;
        return true;
    }
    return false;
}


// 200MP limit (approx 800MB raw buffer). 
// If GIF frames * width * height exceeds this, we fallback to static image (first frame).
#define MORPH_MAX_GIF_PIXELS 200000000 

// Helper: Load & Resize
static vips::VImage morph_transform_load(const std::string& image_data, int image_type, MorphOptions *options) {
    // Unified Optimized Load Logic using thumbnail_buffer
    // This handles both static images and animated GIFs efficiently.
    // It also handles "Smart Resize" (Cover+Crop) via VIPS_INTERESTING_CENTRE.
    
    // Determine if we can use thumbnail (mostly yes if resizing)
    // We force use_thumbnail for GIFs now to use n=-1 optimization
    bool is_gif = (image_type == MORPH_IMG_GIF);
    bool use_thumbnail = false;
    bool force_static = false;

    // Smart Fallback Check for Animated GIFs (DoS Protection)
    if (image_type == MORPH_IMG_GIF) {
        try {
            // Peek header only (fast)
            vips::VOption *peek_opts = vips::VImage::option()->set("n", -1)->set("access", VIPS_ACCESS_SEQUENTIAL);
            vips::VImage peek = vips::VImage::new_from_buffer(image_data.data(), image_data.size(), "", peek_opts);
            
            size_t total_pixels = (size_t)peek.width() * peek.height(); // height includes all frames
            if (total_pixels > MORPH_MAX_GIF_PIXELS) {
                force_static = true;
                if (options->debug) {
                    MorphLogger::instance().debug("GIF too heavy (%lu pixels). Falling back to static image.", total_pixels);
                }
            }
        } catch (...) {
            // If peek fails, proceed with default caution
        }
    }
    
    // Condition for thumbnail:
    // 1. Resizing requested (W or H > 0) AND (Smart Resize OR Standard Resize)
    // 2. We basically always use it for resize if possible.
    if ((options->width > 0 || options->height > 0)) {
         use_thumbnail = true;
    }
    
    if (use_thumbnail) {
        vips::VOption *thumb_opts = vips::VImage::option()->set("no_rotate", false);
        if (options->height > 0) thumb_opts->set("height", options->height);
        
        // GIF Animation Support: Load all frames
        if (is_gif && !force_static) {
            thumb_opts->set("option_string", "n=-1");
        }
        
        // Smart Resize Optimization (Resize to Cover + Crop)
        // If user wants Smart Resize (width & height set, no manual crop), 
        // we use VIPS_INTERESTING_CENTRE to automatically cover and crop.
        // NOTE: For GIFs (n=-1), Vips might fail or crop the strip incorrectly if we use built-in crop.
        // We disable built-in crop for GIFs and let our robust frame-by-frame resize_smart handle it.
        if (!is_gif && !options->has_crop && options->width > 0 && options->height > 0) {
            thumb_opts->set("crop", VIPS_INTERESTING_CENTRE);
        }

        int load_width = options->width > 0 ? options->width : 10000;
        
        // NOTE: If only Height is provided, load_width needs to be huge or handled.
        // vips_thumbnail needs width. If width not set, maybe rely on height constraint?
        // Actually vips_thumbnail requires width. 
        // If we only have height, we set width to very large (10000) so height controls it, 
        // OR we don't use thumbnail for height-only? 
        // Let's stick to current logic: width or 10000.
        
        vips::VImage img = vips::VImage::thumbnail_buffer((void*)image_data.data(), image_data.size(), load_width, thumb_opts);
        
        // Update Actual Loaded Options to reflect what thumbnail did
        // If we asked for Centre Crop, result is already WxH.
        // We shouldn't set options->width/height to 0 yet because filters might need them?
        // But resize step should be skipped if dimensions match.
        
        return img;
    } else {
        // Fallback for no-resize load (Original Image)
        vips::VOption *load_opts = vips::VImage::option();
        if (is_gif && !force_static) {
            load_opts->set("n", -1);
        }
        return vips::VImage::new_from_buffer(image_data.data(), image_data.size(), "", load_opts);
    }
}

// Helper: Apply Geometry (Resize, Crop, etc)
static vips::VImage morph_transform_geometry(vips::VImage image, MorphOptions *options) {
    if (options->width > 0 || options->height > 0) {
        if (options->has_crop) {
            image = morph_resizer_resize(image, options->width, options->height);
        } else {
            image = morph_resizer_resize_smart(image, options->width, options->height, options->gravity);
        }
    }

    if (options->has_crop) {
        image = morph_resizer_crop(image, options->cx, options->cy, options->cw, options->ch);
    }

    if (options->rotate_angle != 0.0) {
        image = morph_resizer_rotate(image, options->rotate_angle);
    }

    if (options->flip) {
        image = morph_resizer_flip(image, options->flip_dir);
    }
    return image;
}

// Helper: Apply Filters
static vips::VImage morph_transform_filters(vips::VImage image, MorphOptions *options) {
    if (!options->bg_color.empty()) image = morph_filters_set_background(image, options->bg_color.c_str());
    if (options->grayscale) image = morph_filters_to_grayscale(image);
    if (options->brightness != 1.0) image = morph_filters_apply_brightness(image, options->brightness);
    if (options->contrast != 1.0) image = morph_filters_apply_contrast(image, options->contrast);
    if (options->blur_sigma > 0.0) image = morph_filters_apply_blur(image, options->blur_sigma);
    if (options->sharpen_sigma > 0.0) image = morph_filters_apply_sharpen(image, options->sharpen_sigma);
    if (options->noise_sigma > 0.0) image = morph_filters_apply_noise(image, 0, options->noise_sigma);
    if (!options->watermark_path.empty()) image = morph_filters_apply_watermark(image, options);
    return image;
}

// Helper: Save Cache
static void morph_save_cache(MorphOptions *options, const char* data, size_t len) {
    std::string cache_path = morph_image_get_cache_path(options);
    if (ensure_directory(cache_path) == 0) {
        std::ofstream outfile(cache_path, std::ios::binary);
        if (outfile) {
            outfile.write(data, len);
            outfile.close();
        }
    }
}

ngx_int_t morph_image_process(MorphOptions *options, std::string *out_data, ngx_log_t *log, bool *is_cache_hit, time_t *last_modified)
{
    if (is_cache_hit) *is_cache_hit = false;
    if (last_modified) *last_modified = 0;

    // 1. Check Cache
    if (morph_check_cache(options, out_data, last_modified)) {
        if (is_cache_hit) *is_cache_hit = true;
        return NGX_OK;
    }

    // 2. Fetch Source
    std::string image_data;
    if (morph_reader_read_source(options, &image_data, log) != NGX_OK) {
        return NGX_HTTP_NOT_FOUND;
    }

    // 3. Validate
    int image_type = MORPH_IMG_UNKNOWN;
    if (morph_image_validate_hex((void*)image_data.data(), image_data.size(), &image_type) != NGX_OK) {
        return NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
    }

    try {
        // 4. Transform Pipeline
        vips::VImage image = morph_transform_load(image_data, image_type, options);
        image = morph_transform_geometry(image, options);
        image = morph_transform_filters(image, options);

        // 5. Output
        vips::VOption *save_opts = vips::VImage::option();
        std::string ext = ".jpg";
        
        // Determine output format
        std::string target_fmt = options->format;
        if (target_fmt.empty()) {
             switch (image_type) {
                case MORPH_IMG_PNG: target_fmt = "png"; break;
                case MORPH_IMG_WEBP: target_fmt = "webp"; break;
                case MORPH_IMG_GIF: target_fmt = "gif"; break;
                default: target_fmt = "jpg"; break;
            }
            // Update options with detected format for correct caching
            options->format = target_fmt;
        }

        if (target_fmt == "png") {
            ext = ".png";
        } else if (target_fmt == "webp") {
            ext = ".webp";
            save_opts->set("Q", options->quality);
        } else if (target_fmt == "gif") {
            ext = ".gif";
        } else {
            ext = ".jpg";
            save_opts->set("Q", options->quality);
        }

        char *buf = NULL;
        size_t len = 0;
        image.write_to_buffer(ext.c_str(), (void**)&buf, &len, save_opts);
        
        if (buf && len > 0) {
            out_data->assign(buf, len);
            morph_save_cache(options, buf, len);
            if (last_modified) *last_modified = time(NULL);
            g_free(buf);
            return NGX_OK;
        }

    } catch (vips::VError &e) {
         ngx_log_error(NGX_LOG_ERR, log, 0, "Vips Processing Error: %s", e.what());
         return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}

ngx_int_t morph_image_validate_hex(void *data, size_t len, int *out_type)
{
    if (out_type) *out_type = MORPH_IMG_UNKNOWN;
    if (len < 12) return NGX_ERROR;

    unsigned char *bytes = (unsigned char *)data;

    // JPEG
    if (bytes[0] == 0xFF && bytes[1] == 0xD8) {
        if (out_type) *out_type = MORPH_IMG_JPEG;
        return NGX_OK;
    }
    // PNG
    if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47 &&
        bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A) {
        if (out_type) *out_type = MORPH_IMG_PNG;
        return NGX_OK;
    }
    // GIF
    if (bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' &&
        bytes[3] == '8' && (bytes[4] == '7' || bytes[4] == '9') && bytes[5] == 'a') {
        if (out_type) *out_type = MORPH_IMG_GIF;
        return NGX_OK;
    }
    // WEBP
    if (bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
        bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P') {
        if (out_type) *out_type = MORPH_IMG_WEBP;
        return NGX_OK;
    }

    return NGX_ERROR;
}