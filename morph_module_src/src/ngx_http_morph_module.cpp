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
#include "ngx_http_morph_types.h"
#include "ngx_http_morph_globals.h"
#include "ngx_http_morph_utils.h"

std::map<std::string, MorphServiceConfig> g_morph_services;
std::string g_morph_config_file_path;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ngx_flag_t  enable;
    ngx_int_t   quality;
    ngx_int_t   webp_quality;
    ngx_int_t   width_min;
    ngx_int_t   width_max;
    ngx_int_t   height_min;
    ngx_int_t   height_max;
    ngx_str_t   service_file;
    ngx_flag_t  debug;
    ngx_flag_t  purge;
} ngx_http_morph_loc_conf_t;

static char* ngx_http_morph( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );
static void* ngx_http_morph_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_morph_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_morph_handler(ngx_http_request_t* r);

static ngx_command_t ngx_http_morph_commands[] = 
{
    { 
        ngx_string( "morph_thumbnail" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_morph,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    { 
        ngx_string( "quality" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, quality),
        NULL
    },
    { 
        ngx_string( "webp_quality" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, webp_quality),
        NULL
    },
    { 
        ngx_string( "width_min" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, width_min),
        NULL
    },
    { 
        ngx_string( "width_max" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, width_max),
        NULL
    },
    { 
        ngx_string( "height_min" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, height_min),
        NULL
    },
    { 
        ngx_string( "height_max" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, height_max),
        NULL
    },
    { 
        ngx_string( "service" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, service_file),
        NULL
    },

    { 
        ngx_string( "morph_debug" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, debug),
        NULL
    },

    {
        ngx_string( "morph_purge" ),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_morph_loc_conf_t, purge),
        NULL
    },

    ngx_null_command
};

static ngx_http_module_t ngx_http_morph_module_ctx = 
{
    NULL,                                                   /* preconfiguration */
    NULL,                                                   /* postconfiguration */
    NULL,                                                   /* create main configuration */
    NULL,                                                   /* init main configuration */
    NULL,                                                   /* create server configuration */
    NULL,                                                   /* merge server configuration */
    ngx_http_morph_create_loc_conf,                         /* create location configuration */
    ngx_http_morph_merge_loc_conf                           /* merge location configuration */
};

static ngx_int_t ngx_http_morph_init_process(ngx_cycle_t *cycle)
{
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "Morph: Failed to initialize Libcurl");
        return NGX_ERROR;
    }

    if (VIPS_INIT("nginx-morph")) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "Morph: Failed to initialize Libvips: %s", vips_error_buffer());
        vips_error_clear();
        return NGX_ERROR;
    }

    vips_concurrency_set(1);
    vips_cache_set_max(0);

    ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "Morph: Worker process initialized (Vips, Curl)");

    std::string log_path((char *)cycle->prefix.data, cycle->prefix.len);
    log_path += "logs/morph_debug.log";
    if (!MorphLogger::instance().open(log_path)) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "Morph: Failed to open debug log: %s", log_path.c_str());
    } else {
        MorphLogger::instance().debug("Morph Module Initialized. Version: 1.0 (PID: %d)", (int)ngx_pid);
    }

    if (!g_morph_config_file_path.empty()) {
        ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "Morph: Loading config from %s", g_morph_config_file_path.c_str());
        
        std::ifstream f(g_morph_config_file_path.c_str());
        if (f.is_open()) {
            try {
                json v = json::parse(f);
                
                if (v.contains("service") && v["service"].is_object()) {
                    auto services = v["service"];
                    for (auto it = services.begin(); it != services.end(); ++it) {
                        std::string service_name = it.key();
                        auto svc_conf = it.value();
                        
                        if (svc_conf.is_object()) {
                            MorphServiceConfig config;
                            
                            if (svc_conf.contains("ttl") && svc_conf["ttl"].is_number()) {
                                config.ttl = svc_conf["ttl"].get<int>();
                            } else {
                                config.ttl = -1;
                            }
                            
                            if (svc_conf.contains("source") && svc_conf["source"].is_array()) {
                                for (const auto& src : svc_conf["source"]) {
                                    if (src.is_string()) {
                                        config.sources.push_back(src.get<std::string>());
                                    }
                                }
                            }
                            
                            g_morph_services[service_name] = config;
                            ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "Morph: Loaded Service '%s' (TTL: %d, Sources: %d)", 
                                service_name.c_str(), config.ttl, config.sources.size());
                        }
                    }
                }
            } catch (json::parse_error& e) {
                 ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "Morph: JSON Parse Error: %s", e.what());
            } catch (json::type_error& e) {
                 ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "Morph: JSON Type Error: %s", e.what());
            }

            f.close();
        } else {
            ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "Morph: Failed to open config file: %s", g_morph_config_file_path.c_str());
        }
    } else {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "Morph: No config file path set (service directive missing?)");
    }

    return NGX_OK;
}

static void ngx_http_morph_exit_process(ngx_cycle_t *cycle)
{
    vips_shutdown();
    curl_global_cleanup();
}

ngx_module_t ngx_http_morph_module = 
{
    NGX_MODULE_V1,
    &ngx_http_morph_module_ctx,                             /* module context */
    ngx_http_morph_commands,                                /* module directives */
    NGX_HTTP_MODULE,                                        /* module type */
    NULL,                                                   /* init master */
    NULL,                                                   /* init module */
    ngx_http_morph_init_process,                            /* init process */
    NULL,                                                   /* init thread */
    NULL,                                                   /* exit thread */
    ngx_http_morph_exit_process,                            /* exit process */
    NULL,                                                   /* exit master */
    NGX_MODULE_V1_PADDING
};

static void* ngx_http_morph_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_morph_loc_conf_t *conf;

    conf = (ngx_http_morph_loc_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_morph_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->quality = NGX_CONF_UNSET;
    conf->webp_quality = NGX_CONF_UNSET;
    conf->width_min = NGX_CONF_UNSET;
    conf->width_max = NGX_CONF_UNSET;
    conf->height_min = NGX_CONF_UNSET;
    conf->height_max = NGX_CONF_UNSET;
    conf->debug = NGX_CONF_UNSET;
    conf->purge = NGX_CONF_UNSET;

    return conf;
}

static char* ngx_http_morph_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_morph_loc_conf_t *prev = (ngx_http_morph_loc_conf_t *)parent;
    ngx_http_morph_loc_conf_t *conf = (ngx_http_morph_loc_conf_t *)child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->quality, prev->quality, 90);
    ngx_conf_merge_value(conf->webp_quality, prev->webp_quality, 80);
    ngx_conf_merge_value(conf->width_min, prev->width_min, 1);
    ngx_conf_merge_value(conf->width_max, prev->width_max, 4000);
    ngx_conf_merge_value(conf->height_min, prev->height_min, 1);
    ngx_conf_merge_value(conf->height_max, prev->height_max, 4000);
    ngx_conf_merge_value(conf->purge, prev->purge, 0);

    // Requests matched by limit_except (e.g. DELETE) run on that block's own core conf, which has
    // no content handler; without this they fall through to the static module and get 405.
    // Same approach as ngx_http_proxy_merge_loc_conf.
    // limit_except에 걸린 요청(예: DELETE)은 그 블록 자체의 core conf로 처리되는데, 거기엔 콘텐츠
    // 핸들러가 없어서 이 처리가 없으면 static 모듈로 넘어가 405가 된다.
    // ngx_http_proxy_merge_loc_conf와 같은 방식이다.
    ngx_http_core_loc_conf_t *clcf = (ngx_http_core_loc_conf_t *)ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    if (clcf->lmt_excpt && clcf->handler == NULL && conf->enable == 1) {
        clcf->handler = ngx_http_morph_handler;
    }
    ngx_conf_merge_str_value(conf->service_file, prev->service_file, "");
    if (conf->service_file.len > 0) {
        if (conf->service_file.data[0] == '/') {
            g_morph_config_file_path.assign((char*)conf->service_file.data, conf->service_file.len);
        } else {
             std::string prefix((char*)cf->cycle->conf_prefix.data, cf->cycle->conf_prefix.len);
             std::string file((char*)conf->service_file.data, conf->service_file.len);
             g_morph_config_file_path = prefix + file;
        }

        std::ifstream f(g_morph_config_file_path.c_str());
        if (!f.is_open()) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "Morph: Config file not found: %s", g_morph_config_file_path.c_str());
            return (char*)NGX_CONF_ERROR;
        }

        std::stringstream buffer;
        buffer << f.rdbuf();
        std::string json_content = buffer.str();
        f.close();

        try {
            json v = json::parse(json_content);
            
            if (!v.is_object() || !v.contains("service")) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "Morph: Config JSON missing 'service' object in %s", g_morph_config_file_path.c_str());
                return (char*)NGX_CONF_ERROR;
            }
        } catch (json::parse_error& e) {
             ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "Morph: Config JSON Syntax Error in %s: %s", g_morph_config_file_path.c_str(), e.what());
             return (char*)NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

// ------------------------------------------------------------------
// Parsing Helpers (Refactoring)
// 파싱 헬퍼
// ------------------------------------------------------------------

// Strict decimal int list ("10,20,30") with range check; sscanf("%d") overflows silently.
// 범위 검사가 있는 엄격한 10진 정수 목록 ("10,20,30"). sscanf("%d")는 오버플로를 알리지 않는다.
static bool parse_int_list(const char *s, char sep, int count, int *out, long lo, long hi) {
    for (int i = 0; i < count; i++) {
        if (!(isdigit((unsigned char)*s) || *s == '-')) return false;
        char *end;
        errno = 0;
        long v = strtol(s, &end, 10);
        if (errno != 0 || end == s || v < lo || v > hi) return false;
        out[i] = (int)v;
        if (i < count - 1) {
            if (*end != sep) return false;
            s = end + 1;
        } else if (*end != '\0') {
            return false;
        }
    }
    return true;
}

static ngx_int_t parse_crop(const std::string& part, MorphOptions& opts) {
    int v[4];
    if (!parse_int_list(part.c_str() + 1, ',', 4, v, -100000, 100000)) return NGX_ERROR;
    opts.cx = v[0]; opts.cy = v[1]; opts.cw = v[2]; opts.ch = v[3];
    opts.has_crop = true;
    return NGX_OK;
}

static ngx_int_t parse_watermark(const std::string& part, MorphOptions& opts) {
    if (part.size() <= 3 || part[1] != '(' || part.back() != ')') return NGX_ERROR;
    std::string content = part.substr(2, part.size() - 3);
    
    std::stringstream ss(content);
    std::string item;
    while(std::getline(ss, item, ',')) {
        size_t colon = item.find(':');
        if (colon == std::string::npos) return NGX_ERROR;
        {
            std::string k = item.substr(0, colon);
            std::string v = item.substr(colon+1);
            
            if (k == "path") opts.watermark_path = MorphUtils::sanitize_path(v);
            else if (k == "o" || k == "opacity") {
                opts.watermark_opacity = std::stod(v);
                if (!(opts.watermark_opacity >= 0.0 && opts.watermark_opacity <= 1.0)) return NGX_ERROR;
            }
            else if (k == "x" || k == "y") {
                int off;
                if (!parse_int_list(v.c_str(), ',', 1, &off, -100000, 100000)) return NGX_ERROR;
                (k == "x" ? opts.watermark_x_offset : opts.watermark_y_offset) = off;
            }
            else if (k == "g") {
                if (v == "c" || v == "center") opts.watermark_gravity = MORPH_GRAVITY_CENTER;
                else if (v == "nw" || v == "tl") opts.watermark_gravity = MORPH_GRAVITY_TOP_LEFT;
                else if (v == "n" || v == "top") opts.watermark_gravity = MORPH_GRAVITY_TOP;
                else if (v == "ne" || v == "tr") opts.watermark_gravity = MORPH_GRAVITY_TOP_RIGHT;
                else if (v == "w" || v == "left") opts.watermark_gravity = MORPH_GRAVITY_LEFT;
                else if (v == "e" || v == "right") opts.watermark_gravity = MORPH_GRAVITY_RIGHT;
                else if (v == "sw" || v == "bl") opts.watermark_gravity = MORPH_GRAVITY_BOTTOM_LEFT;
                else if (v == "s" || v == "bottom") opts.watermark_gravity = MORPH_GRAVITY_BOTTOM;
                else if (v == "se" || v == "br") opts.watermark_gravity = MORPH_GRAVITY_BOTTOM_RIGHT;
                else return NGX_ERROR;
            }
            else return NGX_ERROR;
        }
    }
    // ".", ".." would resolve to a directory, not a file
    // ".", ".."는 파일이 아니라 디렉터리로 해석된다
    if (opts.watermark_path.empty() || opts.watermark_path.find_first_not_of('.') == std::string::npos) return NGX_ERROR;
    return NGX_OK;
}

// Parses a number and checks it is finite and inside [lo, hi]; std::stod throws on garbage.
// 숫자를 파싱하고 유한한 값이며 [lo, hi] 안인지 확인한다. std::stod는 잘못된 입력에 예외를 던진다.
static bool parse_bounded(const std::string& val, double lo, double hi, double& out) {
    double d = std::stod(val);
    if (!std::isfinite(d) || d < lo || d > hi) return false;
    out = d;
    return true;
}

static ngx_int_t parse_single_filter(const std::string& key, const std::string& val, MorphOptions& opts) {
    // Upper bounds keep a single request from pinning a worker (gaussblur cost grows with sigma)
    // 상한을 둬서 요청 하나가 워커를 붙잡지 못하게 한다 (gaussblur 비용은 sigma에 따라 커진다)
    bool ok = true;
    if (key == "bakground_color" || key == "background_color") {
        ok = val.size() == 6 && val.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
        opts.bg_color = val;
    }
    else if (key == "blur" && !val.empty()) { ok = parse_bounded(val, 0.0, 50.0, opts.blur_sigma); }
    else if (key == "sharpen" && !val.empty()) { ok = parse_bounded(val, 0.0, 10.0, opts.sharpen_sigma); }
    else if (key == "format") {
        opts.format = (val == "jpeg") ? "jpg" : val;
        ok = opts.format == "jpg" || opts.format == "png" || opts.format == "webp" || opts.format == "gif";
    }
    else if (key == "grayscale") { opts.grayscale = true; }
    else if (key == "quality" && !val.empty()) { opts.quality = std::stoi(val); ok = opts.quality >= 1 && opts.quality <= 100; }
    else if (key == "rotate" && !val.empty()) { ok = parse_bounded(val, -3600.0, 3600.0, opts.rotate_angle); }
    else if (key == "flip") {
        opts.flip = true;
        ok = val.empty() || val == "v" || val == "h";
        opts.flip_dir = (val == "h") ? 1 : 0;
    }
    else if (key == "brightness" && !val.empty()) { ok = parse_bounded(val, 0.0, 10.0, opts.brightness); }
    else if (key == "contrast" && !val.empty()) { ok = parse_bounded(val, 0.0, 10.0, opts.contrast); }
    else if (key == "noise" && !val.empty()) { ok = parse_bounded(val, 0.0, 100.0, opts.noise_sigma); }
    else { return NGX_ERROR; } 
    return ok ? NGX_OK : NGX_ERROR;
}

static ngx_int_t parse_filter(const std::string& part, MorphOptions& opts) {
    if (part.size() <= 3 || part[1] != '(' || part.back() != ')') return NGX_ERROR;

    std::string content = part.substr(2, part.size() - 3);
    size_t colon_pos = content.find(':');
    std::string key = (colon_pos == std::string::npos) ? content : content.substr(0, colon_pos);
    std::string val = (colon_pos == std::string::npos) ? "" : content.substr(colon_pos + 1);

    return parse_single_filter(key, val, opts);
}

static ngx_int_t parse_gravity(const std::string& part, MorphOptions& opts) {
    if (part.size() <= 3 || part[1] != '(' || part.back() != ')') return NGX_ERROR;
    std::string val = part.substr(2, part.size() - 3);
    
    if (val == "ctr" || val == "center") opts.gravity = MORPH_GRAVITY_CENTER;
    else if (val == "top" || val == "north") opts.gravity = MORPH_GRAVITY_TOP;
    else if (val == "bottom" || val == "south") opts.gravity = MORPH_GRAVITY_BOTTOM;
    else if (val == "left" || val == "west") opts.gravity = MORPH_GRAVITY_LEFT;
    else if (val == "right" || val == "east") opts.gravity = MORPH_GRAVITY_RIGHT;
    else return NGX_ERROR;
    
    return NGX_OK;
}

static ngx_int_t parse_dimension(const std::string& part, MorphOptions& opts) {
    int v[2];
    // range is enforced again against width_max/height_max in the handler
    // 범위는 핸들러에서 width_max/height_max로 다시 검사한다
    if (!parse_int_list(part.c_str(), 'x', 2, v, 0, 100000)) return NGX_ERROR;
    opts.width = v[0];
    opts.height = v[1];
    return NGX_OK;
}

// Find the next '_' separator outside parentheses, so values like
// F(background_color:fff) or W(path:my_logo.png) stay in one part.
// 괄호 밖에 있는 다음 '_' 구분자를 찾는다. 그래서 F(background_color:fff)나
// W(path:my_logo.png) 같은 값이 한 덩어리로 유지된다.
static size_t find_separator(const std::string& s, size_t from) {
    int depth = 0;
    for (size_t i = from; i < s.size(); i++) {
        if (s[i] == '(') depth++;
        else if (s[i] == ')' && depth > 0) depth--;
        else if (s[i] == '_' && depth == 0) return i;
    }
    return std::string::npos;
}

// Parse the URL option segment to extract image processing parameters.
// URL의 옵션 구간을 파싱해서 이미지 처리 파라미터를 뽑는다.
static ngx_int_t parse_options(const std::string& segment, MorphOptions& opts) {
    size_t start = 0;
    size_t end = find_separator(segment, 0);
    
    while (true) {
        std::string part;
        if (end == std::string::npos) {
            part = segment.substr(start);
        } else {
            part = segment.substr(start, end - start);
        }

        if (!part.empty()) {
            ngx_int_t res = NGX_OK;
            if (part[0] == 'C') res = parse_crop(part, opts);
            else if (part[0] == 'W') res = parse_watermark(part, opts);
            else if (part[0] == 'F') res = parse_filter(part, opts);
            else if (part[0] == 'G') res = parse_gravity(part, opts);
            else if (isdigit(part[0])) res = parse_dimension(part, opts);
            else res = NGX_ERROR;

            if (res != NGX_OK) return res;
        }

        if (end == std::string::npos) break;
        start = end + 1;
        end = find_separator(segment, start);
    }
    return NGX_OK;
}

struct MorphThreadCtx {
    MorphOptions options;
    std::string output_data;
    ngx_http_request_t *r;
    ngx_int_t status;
    bool is_cache_hit;
    time_t if_modified_since;
    time_t last_modified;
    
    MorphThreadCtx() : r(NULL), status(NGX_OK), is_cache_hit(false), if_modified_since(-1), last_modified(0) {}
};

static void morph_thread_func(void *data, ngx_log_t *log)
{
    MorphThreadCtx *ctx = (MorphThreadCtx *)data;
    if (ctx->options.debug) {
        MorphLogger::instance().debug("Thread Started: %s (PID: %d)", ctx->options.source_path.c_str(), (int)ngx_pid);
    }
    try {
        ctx->status = morph_image_process(&ctx->options, &ctx->output_data, log, &ctx->is_cache_hit, &ctx->last_modified);
    } catch (...) {
        // An exception escaping the thread pool task would terminate the worker
        // 스레드풀 작업 밖으로 예외가 나가면 워커가 종료된다
        ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Unexpected exception: %s", ctx->options.source_path.c_str());
        ctx->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
}

static void morph_thread_completion(ngx_event_t *ev)
{
    ngx_thread_task_t *task = (ngx_thread_task_t *)ev->data;
    MorphThreadCtx *ctx = (MorphThreadCtx *)task->ctx;
    ngx_http_request_t *r = ctx->r;
    
    if (ctx->status != NGX_OK) {
        ngx_http_finalize_request(r, ctx->status);
        delete ctx;
        return;
    }

    if (ctx->if_modified_since != -1 && ctx->last_modified > 0) {
        if (ctx->options.debug) {
            MorphLogger::instance().debug("304 Check: Last-Mod=%ld, If-Mod=%ld", (long)ctx->last_modified, (long)ctx->if_modified_since);
        }
        
        if (ctx->last_modified <= ctx->if_modified_since) {
            r->headers_out.status = NGX_HTTP_NOT_MODIFIED;
            r->headers_out.last_modified_time = ctx->last_modified;
            
            ngx_http_finalize_request(r, NGX_HTTP_NOT_MODIFIED);
            delete ctx;
            return;
        }
    }

    size_t len = ctx->output_data.size();
    if (len == 0) {
        ngx_http_finalize_request(r, NGX_HTTP_NO_CONTENT);
        delete ctx;
        return;
    }

    ngx_buf_t *b = (ngx_buf_t*)ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        delete ctx;
        return;
    }

    u_char *payload = (u_char*)ngx_palloc(r->pool, len);
    if (payload == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        delete ctx;
        return;
    }

    ngx_memcpy(payload, ctx->output_data.data(), len);
    
    b->pos = payload;
    b->last = payload + len;
    b->memory = 1;
    b->last_buf = 1;

    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;

    std::string fmt = ctx->options.format;
    if (fmt.empty()) fmt = "jpg"; 

    if (fmt == "png") {
        ngx_str_set(&r->headers_out.content_type, "image/png");
    } else if (fmt == "webp") {
        ngx_str_set(&r->headers_out.content_type, "image/webp");
    } else if (fmt == "gif") {
        ngx_str_set(&r->headers_out.content_type, "image/gif");
    } else {
        ngx_str_set(&r->headers_out.content_type, "image/jpeg");
    }

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = len;
    
    if (ctx->last_modified > 0) {
        r->headers_out.last_modified_time = ctx->last_modified;
    }

    ngx_table_elt_t *h = (ngx_table_elt_t *)ngx_list_push(&r->headers_out.headers);
    if (h) {
        h->hash = 1;
        ngx_str_set(&h->key, "X-Cache-Status");
        if (ctx->is_cache_hit) {
            ngx_str_set(&h->value, "HIT");
        } else {
            ngx_str_set(&h->value, "MISS");
        }
    }

    if (ctx->options.debug) {
        std::string client_ip((char*)r->connection->addr_text.data, r->connection->addr_text.len);
        MorphLogger::instance().debug("Status: OK, Client IP: %s", client_ip.c_str());
    }
    delete ctx;

    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) { // HEAD: headers only / HEAD면 헤더만
        ngx_http_finalize_request(r, rc);
        return;
    }
    ngx_http_finalize_request(r, ngx_http_output_filter(r, &out));
}

static ngx_int_t ngx_http_morph_handler( ngx_http_request_t* r )
{
    ngx_http_morph_loc_conf_t *cf;
    cf = (ngx_http_morph_loc_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_morph_module);

    if (cf->enable == 0) {
        return NGX_DECLINED;
    }

    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD && r->method != NGX_HTTP_DELETE) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    // Purge is opt-in: without "morph_purge on" anyone reaching the location could wipe the cache.
    // purge는 명시적으로 켜야 한다: "morph_purge on" 없이 열어 두면 location에 닿는 누구나 캐시를 지울 수 있다.
    if (r->method == NGX_HTTP_DELETE && cf->purge != 1) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    // An unread body would be parsed as the next request on a keep-alive connection
    // 읽지 않은 본문은 keep-alive 연결에서 다음 요청으로 해석된다
    ngx_int_t body_rc = ngx_http_discard_request_body(r);
    if (body_rc != NGX_OK) {
        return body_rc;
    }

    std::string uri((char*)r->uri.data, r->uri.len);

    if (uri.find("/morph/") == 0) {
        uri = uri.substr(7);
    } else if (uri.find("/") == 0) {
        uri = uri.substr(1);
    }
    
    size_t first_slash = uri.find('/');
    size_t second_slash = uri.find('/', first_slash + 1);
    
    if (first_slash == std::string::npos || second_slash == std::string::npos || second_slash + 1 >= uri.size()) {
        return NGX_HTTP_BAD_REQUEST; // also rejects an empty source path / 빈 원본 경로도 거부
    }
    
    std::string service_name = uri.substr(0, first_slash);
    std::string options_str = uri.substr(first_slash + 1, second_slash - (first_slash + 1));
    std::string source_path = uri.substr(second_slash + 1); 
    
    ngx_http_core_loc_conf_t *clcf = (ngx_http_core_loc_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    std::string doc_root((char*)clcf->root.data, clcf->root.len);

    MorphOptions options;
    ngx_int_t parse_rc;
    try {
        parse_rc = parse_options(options_str, options);
    } catch (const std::exception&) {
        // std::stod/stoi on bad input (e.g. "F(blur:abc)") would otherwise abort the worker
        // 잘못된 입력(예: "F(blur:abc)")에서 std::stod/stoi가 던진 예외가 워커를 죽이지 않게 한다
        parse_rc = NGX_ERROR;
    }
    if (parse_rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Morph: Invalid options format: %s", options_str.c_str());
        return NGX_HTTP_BAD_REQUEST;
    }

    // 0 means "not set" (e.g. "300x0" keeps aspect ratio)
    // 0은 "지정 안 함" (예: "300x0"은 비율 유지)
    if ((options.width != 0 && (options.width < cf->width_min || options.width > cf->width_max)) ||
        (options.height != 0 && (options.height < cf->height_min || options.height > cf->height_max))) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Morph: Size out of range: %dx%d", options.width, options.height);
        return NGX_HTTP_BAD_REQUEST;
    }
    options.source_path = source_path;
    options.service_name = service_name;
    options.document_root = doc_root;
    options.raw_options = options_str;

    // Resolved before the cache path: the effective quality is part of the cache key.
    // An explicit F(quality:N) wins over both directives.
    // 캐시 경로보다 먼저 정한다: 실제 적용 quality가 캐시 키의 일부다.
    // 명시한 F(quality:N)이 두 지시어보다 우선한다.
    options.webp_quality = options.quality != 0 ? options.quality : cf->webp_quality;
    if (options.quality == 0) options.quality = cf->quality;
    
    if (r->method == NGX_HTTP_DELETE) {
        std::string cache_path = morph_image_get_cache_path(&options);
        
        if (unlink(cache_path.c_str()) == 0) {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "Morph: Purged cache file: %s", cache_path.c_str());
            r->headers_out.status = NGX_HTTP_NO_CONTENT;
            return ngx_http_send_header(r); 
        } else {
             ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "Morph: Purge failed (not found?): %s", cache_path.c_str());
             return NGX_HTTP_NOT_FOUND;
        }
    }

    MorphThreadCtx *ctx = new MorphThreadCtx();
    ctx->r = r;
    ctx->options = options;
    
    if (r->headers_in.if_modified_since) {
        ctx->if_modified_since = ngx_http_parse_time(r->headers_in.if_modified_since->value.data, r->headers_in.if_modified_since->value.len);
    }
    
    ctx->options.debug = cf->debug == 1;

    if (ctx->options.debug) {
        std::string req_url((char*)r->uri.data, r->uri.len);
        MorphLogger::instance().debug("Request URL: %s", req_url.c_str());
    }

    if (ctx->options.debug) {
        MorphLogger::instance().debug("User Options: Width=%d, Height=%d, Crop=%d, Grayscale=%d, Blur=%.1f, Rotate=%.1f, Flip=%d, Src=%s", 
            ctx->options.width, ctx->options.height, ctx->options.has_crop, ctx->options.grayscale, 
            ctx->options.blur_sigma, ctx->options.rotate_angle, ctx->options.flip, ctx->options.source_path.c_str());
    }

    ngx_thread_task_t *task = ngx_thread_task_alloc(r->pool, 0);
    if (task == NULL) {
        delete ctx;
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    task->handler = morph_thread_func;
    task->ctx = ctx;
    task->event.handler = morph_thread_completion;
    task->event.data = task;

    if (clcf->thread_pool == NULL) {
         if (ctx->options.debug) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Morph Debug: No thread pool configured in location");
         }
         delete ctx;
         return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_thread_task_post(clcf->thread_pool, task) != NGX_OK) {
        delete ctx;
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->main->count++;

    return NGX_DONE; 
}

static char* ngx_http_morph( ngx_conf_t* cf, ngx_command_t* cmd, void* conf )
{
    ngx_http_core_loc_conf_t* core_location_conf = ( ngx_http_core_loc_conf_t* )ngx_http_conf_get_module_loc_conf( cf, ngx_http_core_module );
    core_location_conf->handler = ngx_http_morph_handler;

    ngx_http_morph_loc_conf_t *mconf = (ngx_http_morph_loc_conf_t *)conf;
    mconf->enable = 1;

    return NGX_CONF_OK;
}

#ifdef __cplusplus
}
#endif