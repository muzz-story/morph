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
#include "ngx_http_morph_image.h"
#include "ngx_http_morph_reader.h"
#include "ngx_http_morph_resizer.h"
#include "ngx_http_morph_filters.h"
#include "ngx_http_morph_utils.h"
#include "ngx_http_morph_globals.h"
#include <sys/file.h>

using namespace MorphUtils;

std::string morph_image_get_cache_path(MorphOptions* options) {
    std::string path = options->document_root;
    if (path.back() != '/') path += "/";
    
    path += options->service_name + "/";
    // Injective encoding + effective quality: "W(path:a,g:se)" and "W(path:a:g:se)" must not share a file,
    // and locations with different quality directives must not serve each other's output.
    // 일대일 인코딩 + 실제 적용 quality: "W(path:a,g:se)"와 "W(path:a:g:se)"는 파일을 공유하면 안 되고,
    // quality 지시어가 다른 location끼리 서로의 결과를 내주면 안 된다.
    path += escape_component(options->raw_options) + "@q" + std::to_string(options->quality)
          + "-" + std::to_string(options->webp_quality) + "/";
    
    if (is_url(options->source_path)) {
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
// 헬퍼: 캐시 확인
static bool morph_check_cache(const std::string& cache_path, MorphOptions *options, std::string *out_data, time_t *last_modified) {
    struct stat st;
    // a directory can sit at this path when another source name is a prefix ("a" vs "a/b.png")
    // 다른 원본 이름이 접두어이면("a"와 "a/b.png") 이 경로에 디렉터리가 있을 수 있다
    if (stat(cache_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;

    // Check TTL
    // TTL 확인
    if (g_morph_services.find(options->service_name) != g_morph_services.end()) {
        int ttl = g_morph_services[options->service_name].ttl;
        if (ttl != -1 && (time(NULL) - st.st_mtime) > ttl) {
            unlink(cache_path.c_str());
            return false;
        }
    }

    // Hit
    // 캐시 적중
    std::ifstream file(cache_path, std::ios::binary);
    if (file) {
        out_data->assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (last_modified) *last_modified = st.st_mtime;
        return true;
    }
    return false;
}


// 200MP limit (approx 800MB raw buffer).
// If GIF frames * frame pixels (source or output, whichever is larger) exceeds this, fall back to the first frame.
// 200MP 상한 (raw 버퍼 약 800MB).
// GIF 프레임 수 * 프레임 픽셀(원본과 출력 중 큰 쪽)이 이를 넘으면 첫 프레임만 쓴다.
#define MORPH_MAX_GIF_PIXELS 200000000
// shortcut: fixed caps, make them directives if real traffic needs larger images
// 고정 상한. 실제 트래픽에 더 큰 이미지가 필요하면 지시어로 바꾼다
#define MORPH_MAX_GIF_FRAMES 1000          // per-frame ops build one pipeline per frame / 프레임별 연산은 프레임마다 파이프라인을 하나씩 만든다
#define MORPH_MAX_SOURCE_PIXELS 150000000  // single frame; decompression-bomb guard (a 14MB JPEG can be 900MP) / 단일 프레임 기준, 압축 폭탄 방지 (14MB JPEG가 900MP일 수 있다)

// Header-only size checks before decoding. Returns 413 for an oversized frame. *animate tells whether
// all GIF frames are loaded: only for GIF/WebP output (jpg/png would save the whole frame strip) and
// only while the frame count and total pixels stay under the caps.
// 디코딩 전에 헤더만 보고 크기를 검사한다. 프레임이 너무 크면 413을 돌려준다. *animate는 GIF의
// 모든 프레임을 읽을지 알려준다: 출력이 GIF/WebP일 때만 (jpg/png는 프레임 띠 전체를 저장하게 됨),
// 그리고 프레임 수와 총 픽셀이 상한 이하일 때만 읽는다.
static ngx_int_t morph_check_dimensions(const std::string& image_data, int image_type, MorphOptions *options, bool *animate) {
    *animate = image_type == MORPH_IMG_GIF &&
               (options->format.empty() || options->format == "gif" || options->format == "webp");

    vips::VImage header = vips::VImage::new_from_buffer(image_data.data(), image_data.size(), "");
    double w = header.width(), h = header.height();
    if (w * h > MORPH_MAX_SOURCE_PIXELS) return NGX_HTTP_REQUEST_ENTITY_TOO_LARGE;
    if (!*animate) return NGX_OK;

    double out_w = options->width, out_h = options->height;
    if (out_w == 0 && out_h == 0) { out_w = w; out_h = h; }
    else if (out_w == 0) out_w = w * out_h / h;
    else if (out_h == 0) out_h = h * out_w / w;

    int frames = header.get_typeof("n-pages") != 0 ? header.get_int("n-pages") : 1;
    double total = std::max(w * h, out_w * out_h) * frames;
    if (frames > MORPH_MAX_GIF_FRAMES || total > MORPH_MAX_GIF_PIXELS) {
        *animate = false;
        if (options->debug) {
            MorphLogger::instance().debug("GIF too heavy (%d frames, %.0f pixels). Falling back to first frame.", frames, total);
        }
    }
    return NGX_OK;
}

// Map gravity to the thumbnail crop mode. Only one axis is cropped after a cover scale,
// so top/bottom matter only when the source is taller than the target, left/right only when wider.
// gravity를 thumbnail 크롭 모드로 바꾼다. cover 스케일 뒤에는 한 축만 잘리므로,
// top/bottom은 원본이 대상보다 세로로 길 때만, left/right는 가로로 길 때만 의미가 있다.
static VipsInteresting morph_thumbnail_interesting(const std::string& image_data, MorphOptions *options) {
    if (options->gravity == MORPH_GRAVITY_CENTER) return VIPS_INTERESTING_CENTRE;

    vips::VImage header = vips::VImage::new_from_buffer(image_data.data(), image_data.size(), "");
    double w = header.width(), h = header.height();
    // EXIF orientation 5-8 is a 90/270 rotation, which thumbnail applies before cropping
    // EXIF orientation 5-8은 90/270도 회전이고, thumbnail이 크롭 전에 적용한다
    if (header.get_typeof("orientation") != 0 && header.get_int("orientation") >= 5) std::swap(w, h);
    bool crops_vertically = w / h < (double)options->width / options->height;

    switch (options->gravity) {
        case MORPH_GRAVITY_TOP:    return crops_vertically ? VIPS_INTERESTING_LOW : VIPS_INTERESTING_CENTRE;
        case MORPH_GRAVITY_BOTTOM: return crops_vertically ? VIPS_INTERESTING_HIGH : VIPS_INTERESTING_CENTRE;
        case MORPH_GRAVITY_LEFT:   return crops_vertically ? VIPS_INTERESTING_CENTRE : VIPS_INTERESTING_LOW;
        case MORPH_GRAVITY_RIGHT:  return crops_vertically ? VIPS_INTERESTING_CENTRE : VIPS_INTERESTING_HIGH;
        default:                   return VIPS_INTERESTING_CENTRE;
    }
}

// Helper: Configure thumbnail load options
// 헬퍼: thumbnail 로드 옵션 설정
static void morph_configure_thumbnail(vips::VOption *thumb_opts, const std::string& image_data, MorphOptions *options, bool animate) {
    thumb_opts->set("no_rotate", false);
    // Unset height must not default to the width (vips would fit a portrait into a WxW box)
    // height를 안 줬을 때 width가 기본값이 되면 안 된다 (세로 이미지가 WxW 상자에 맞춰짐)
    thumb_opts->set("height", options->height > 0 ? options->height : VIPS_MAX_COORD);

    // GIF Animation Support: Load all frames
    // GIF 애니메이션 지원: 모든 프레임 로드
    if (animate) {
        thumb_opts->set("option_string", "n=-1");
    }

    // Smart Resize Optimization (Resize to Cover + Crop)
    // NOTE: Disabled for animations; morph_resizer_resize_smart crops those frame by frame
    // 스마트 리사이즈 최적화 (cover 리사이즈 + 크롭)
    // 참고: 애니메이션에는 쓰지 않는다. morph_resizer_resize_smart가 프레임별로 자른다
    if (!animate && options->width > 0 && options->height > 0) {
        thumb_opts->set("crop", morph_thumbnail_interesting(image_data, options));
    }
}

// Helper: Detect Output Format
// 헬퍼: 출력 포맷 결정
static void morph_detect_output_format(MorphOptions *options, int image_type) {
    if (!options->format.empty()) return;

    std::string target_fmt;
     switch (image_type) {
        case MORPH_IMG_PNG: target_fmt = "png"; break;
        case MORPH_IMG_WEBP: target_fmt = "webp"; break;
        case MORPH_IMG_GIF: target_fmt = "gif"; break;
        default: target_fmt = "jpg"; break;
    }
    options->format = target_fmt;
}

// Helper: Load and resize image from buffer
// 헬퍼: 버퍼에서 이미지를 로드하고 리사이즈
static vips::VImage morph_transform_load(const std::string& image_data, MorphOptions *options, bool animate) {
    bool use_thumbnail = false;

    // Manual crop coordinates refer to the original image, so it must not be shrunk on load
    // 수동 크롭 좌표는 원본 기준이므로 로드할 때 줄이면 안 된다
    if ((options->width > 0 || options->height > 0) && !options->has_crop) {
         use_thumbnail = true;
    }
    
    if (use_thumbnail) {
        vips::VOption *thumb_opts = vips::VImage::option();
        morph_configure_thumbnail(thumb_opts, image_data, options, animate);

        int load_width = options->width > 0 ? options->width : VIPS_MAX_COORD;
        vips::VImage img = vips::VImage::thumbnail_buffer((void*)image_data.data(), image_data.size(), load_width, thumb_opts);
        return img;
    } else {
        // Fallback for no-resize load (Original Image)
        // 리사이즈 없는 로드 (원본 크기)
        vips::VOption *load_opts = vips::VImage::option();
        if (animate) {
            load_opts->set("n", -1);
        }
        vips::VImage img = vips::VImage::new_from_buffer(image_data.data(), image_data.size(), "", load_opts);
        return img.autorot(); // thumbnail_buffer does this itself / thumbnail_buffer는 자체적으로 회전한다
    }
}

// Helper: Apply Geometry (Crop, then Resize, Rotate, Flip)
// 헬퍼: 기하 변환 적용 (크롭 후 리사이즈, 회전, 뒤집기)
static vips::VImage morph_transform_geometry(vips::VImage image, MorphOptions *options) {
    if (options->has_crop) {
        image = morph_resizer_crop(image, options->cx, options->cy, options->cw, options->ch);
    }

    if (options->width > 0 && options->height > 0) {
        image = morph_resizer_resize_smart(image, options->width, options->height, options->gravity);
    } else if (options->has_crop && (options->width > 0 || options->height > 0)) {
        image = morph_resizer_resize(image, options->width, options->height); // thumbnail was skipped / thumbnail을 건너뛰었으므로
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
// 헬퍼: 필터 적용
static vips::VImage morph_transform_filters(vips::VImage image, MorphOptions *options) {
    if (!options->bg_color.empty()) image = morph_filters_set_background(image, options->bg_color.c_str());
    if (options->grayscale) image = morph_filters_to_grayscale(image);
    if (options->brightness != 1.0) image = morph_filters_apply_brightness(image, options->brightness);
    if (options->contrast != 1.0) image = morph_filters_apply_contrast(image, options->contrast);
    // Neighbourhood filters run per frame so animated GIF frames do not bleed into each other
    // 주변 픽셀을 쓰는 필터는 프레임별로 돌려서 GIF 프레임끼리 번지지 않게 한다
    if (options->blur_sigma > 0.0) {
        image = morph_resizer_per_frame(image, [&](vips::VImage f) { return morph_filters_apply_blur(f, options->blur_sigma); });
    }
    if (options->sharpen_sigma > 0.0) {
        image = morph_resizer_per_frame(image, [&](vips::VImage f) { return morph_filters_apply_sharpen(f, options->sharpen_sigma); });
    }
    if (options->noise_sigma > 0.0) image = morph_filters_apply_noise(image, options->noise_sigma);
    if (!options->watermark_path.empty()) image = morph_filters_apply_watermark(image, options);
    return image;
}

// Helper: Save Cache. Written to a temp file and renamed so concurrent readers never see a partial image.
// 헬퍼: 캐시 저장. 임시 파일에 쓴 뒤 rename해서, 동시에 읽는 쪽이 덜 쓰인 이미지를 보지 않는다.
static void morph_save_cache(const std::string& cache_path, const char* data, size_t len) {
    if (ensure_directory(cache_path) != 0) return;

    std::string tmp_path = cache_path + ".XXXXXX";
    int fd = mkstemp(&tmp_path[0]);
    if (fd < 0) return;

    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, data + written, len - written);
        if (n <= 0) break;
        written += (size_t)n;
    }
    fchmod(fd, 0644);
    bool ok = close(fd) == 0 && written == len;

    if (!ok || rename(tmp_path.c_str(), cache_path.c_str()) != 0) {
        unlink(tmp_path.c_str());
    }
}

// Steps 2-5 of a cache miss: fetch, validate, transform, encode and save.
// 캐시 MISS의 2-5단계: 원본 가져오기, 검증, 변환, 인코딩, 저장.
static ngx_int_t morph_image_render(MorphOptions *options, const std::string& cache_path, std::string *out_data, ngx_log_t *log, time_t *last_modified)
{
    // 2. Fetch Source
    // 2. 원본 가져오기
    std::string image_data;
    ngx_int_t read_rc = morph_reader_read_source(options, &image_data, log);
    if (read_rc != NGX_OK) {
        return read_rc; // NGX_HTTP_NOT_FOUND or NGX_HTTP_FORBIDDEN (SSRF / traversal) / 404 또는 403 (SSRF / 경로 조작)
    }

    // 3. Validate
    // 3. 검증
    int image_type = MORPH_IMG_UNKNOWN;
    if (morph_image_validate_hex((void*)image_data.data(), image_data.size(), &image_type) != NGX_OK) {
        return NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
    }

    // A missing watermark is an error, not a reason to serve (and cache) an unwatermarked image
    // 워터마크 파일이 없으면 오류다. 워터마크 없는 이미지를 내주거나 캐시하면 안 된다
    if (!options->watermark_path.empty() && access(morph_filters_watermark_file(options).c_str(), R_OK) != 0) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Watermark not found: %s", morph_filters_watermark_file(options).c_str());
        return NGX_HTTP_NOT_FOUND;
    }

    try {
        // 4. Transform Pipeline
        // 4. 변환 파이프라인
        bool animate = false;
        ngx_int_t size_rc = morph_check_dimensions(image_data, image_type, options, &animate);
        if (size_rc != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Source image too large: %s", options->source_path.c_str());
            return size_rc;
        }
        vips::VImage image = morph_transform_load(image_data, options, animate);
        image = morph_transform_geometry(image, options);
        image = morph_transform_filters(image, options);

        // 5. Output
        // 5. 출력
        vips::VOption *save_opts = vips::VImage::option();
        std::string ext = ".jpg";
        
        morph_detect_output_format(options, image_type);
        std::string target_fmt = options->format;

        if (target_fmt == "png") {
            ext = ".png";
        } else if (target_fmt == "webp") {
            ext = ".webp";
            save_opts->set("Q", options->webp_quality);
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
            morph_save_cache(cache_path, buf, len);
            if (last_modified) *last_modified = time(NULL);
            g_free(buf);
            return NGX_OK;
        }

    } catch (vips::VError &e) {
         ngx_log_error(NGX_LOG_ERR, log, 0, "Vips Processing Error: %s", e.what());
         vips_error_clear(); // the buffer is per thread; stale text would prefix later errors / 오류 버퍼는 스레드별이라, 지우지 않으면 이후 오류 앞에 남은 내용이 붙는다
         return NGX_HTTP_INTERNAL_SERVER_ERROR;
    } catch (std::exception &e) {
         ngx_log_error(NGX_LOG_ERR, log, 0, "Morph Processing Error: %s", e.what());
         return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}

// shortcut: fixed cap, covers a few dead origins (15 s curl timeout each); make it a directive if renders run longer
// 고정 상한, 죽은 origin 몇 개(각 curl 타임아웃 15초)를 버틴다. 렌더가 더 오래 걸리면 지시어로 바꾼다
#define MORPH_LOCK_WAIT_MS 60000
#define MORPH_LOCK_POLL_MS 20

// Single-flight for identical misses across workers and thread-pool threads. One lock file per entry under
// {root}/{service}/@lock/ (the reader refuses that folder as a source). The request holding it renders; the
// rest poll for it (bounded, then render unlocked) and take its cache file, or the error code it wrote into
// the lock file. The holder unlinks the file before closing, so a waiter that gets the lock afterwards sees
// st_nlink == 0: it knows the holder is done and never unlinks the newer file a later request created.
// The kernel drops the lock if a worker dies.
// 워커와 스레드풀 스레드를 아우르는 동일 MISS 단일 처리(single-flight). 캐시 항목마다
// {root}/{service}/@lock/ 아래에 잠금 파일 하나 (reader는 이 폴더를 원본으로 읽지 않는다). 잠금을 잡은
// 요청이 렌더하고, 나머지는 폴링하며 기다렸다가(상한이 있고, 넘으면 잠금 없이 렌더) 그 캐시 파일이나
// 잠금 파일에 남긴 오류 코드를 가져간다. 잡은 쪽은 닫기 전에 파일을 unlink하므로, 그 뒤 잠금을 얻은
// 대기자는 st_nlink == 0을 보고 앞 요청이 끝났음을 알며, 나중 요청이 만든 새 파일은 지우지 않는다.
// 워커가 죽으면 커널이 잠금을 푼다.
struct MorphRenderLock {
    std::string path;
    int fd = -1;
    ngx_int_t shared_rc = NGX_OK;  // error the previous holder finished with / 앞 잠금 보유자가 남긴 오류 코드

    MorphRenderLock(MorphOptions *options, const std::string& cache_path) {
        path = options->document_root;
        if (path.back() != '/') path += "/";
        path += options->service_name + "/@lock/" + compute_md5(cache_path);
        if (ensure_directory(path) != 0) return;
        fd = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0644);
        if (fd < 0) return;
        for (int waited = 0; flock(fd, LOCK_EX | LOCK_NB) != 0; waited += MORPH_LOCK_POLL_MS) {
            if (errno != EWOULDBLOCK || waited >= MORPH_LOCK_WAIT_MS) { close(fd); fd = -1; return; }  // render unlocked / 잠금 없이 렌더
            usleep(MORPH_LOCK_POLL_MS * 1000);
        }
        int rc;
        if (is_released() && pread(fd, &rc, sizeof(rc), 0) == (ssize_t)sizeof(rc)) shared_rc = rc;
    }
    bool is_released() {
        struct stat st;
        return fstat(fd, &st) == 0 && st.st_nlink == 0;
    }
    // Hand a render error to the requests queued on this file (written before the unlink in the destructor)
    // 이 파일에서 기다리는 요청들에게 렌더 오류를 넘긴다 (소멸자의 unlink보다 먼저 기록)
    void record(ngx_int_t rc) {
        int v = (int)rc;
        if (fd >= 0 && rc != NGX_OK && pwrite(fd, &v, sizeof(v), 0) != (ssize_t)sizeof(v)) { /* waiters re-render / 대기자가 다시 렌더 */ }
    }
    ~MorphRenderLock() {
        if (fd < 0) return;
        if (!is_released()) unlink(path.c_str());  // still linked = the file at path is ours / 아직 링크됨 = path의 파일이 내 것
        close(fd);
    }
    MorphRenderLock(const MorphRenderLock&) = delete;
    MorphRenderLock& operator=(const MorphRenderLock&) = delete;
};

ngx_int_t morph_image_process(MorphOptions *options, std::string *out_data, ngx_log_t *log, bool *is_cache_hit, time_t *last_modified)
{
    if (is_cache_hit) *is_cache_hit = false;
    if (last_modified) *last_modified = 0;

    // Computed before morph_detect_output_format() fills options->format, so check, save
    // and DELETE purge all resolve the same file (URL sources used to save as .png but look up .jpg).
    // morph_detect_output_format()이 options->format을 채우기 전에 계산해서, 확인/저장/DELETE purge가
    // 모두 같은 파일을 가리킨다 (URL 원본을 .png로 저장하고 .jpg로 찾던 문제).
    std::string cache_path = morph_image_get_cache_path(options);

    auto serve_cached = [&]() {
        // Content-Type follows the cached bytes when no format was requested
        // 포맷을 지정하지 않았으면 Content-Type은 캐시된 바이트를 따른다
        int cached_type = MORPH_IMG_UNKNOWN;
        morph_image_validate_hex((void*)out_data->data(), out_data->size(), &cached_type);
        morph_detect_output_format(options, cached_type);
        if (is_cache_hit) *is_cache_hit = true;
        return (ngx_int_t)NGX_OK;
    };

    // 1. Check Cache
    // 1. 캐시 확인
    if (morph_check_cache(cache_path, options, out_data, last_modified)) return serve_cached();

    // Another request may be rendering this entry: wait for it, then reuse its result
    // 다른 요청이 이 항목을 렌더 중일 수 있다: 기다렸다가 그 결과를 재사용한다
    MorphRenderLock lock(options, cache_path);
    if (lock.shared_rc != NGX_OK) return lock.shared_rc;
    if (lock.fd >= 0 && morph_check_cache(cache_path, options, out_data, last_modified)) return serve_cached();

    ngx_int_t rc = morph_image_render(options, cache_path, out_data, log, last_modified);
    lock.record(rc);
    return rc;
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