#include "std.h"
#include "ngx_http_morph_image.h"
#include "ngx_http_morph_types.h"
#include "ngx_http_morph_globals.h"
#include "ngx_http_morph_utils.h"

// 전역 변수 정의 (Global Definitions)
std::map<std::string, MorphServiceConfig> g_morph_services;
std::string g_morph_config_file_path;

#ifdef __cplusplus
extern "C" {
#endif

// Configuration Structure / 설정 구조체
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
} ngx_http_morph_loc_conf_t;

static char* ngx_http_morph( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );
static void* ngx_http_morph_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_morph_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

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
// ------------------------------------------------------------------

static ngx_int_t parse_crop(const std::string& part, MorphOptions& opts) {
    if (sscanf(part.c_str(), "C%d,%d,%d,%d", &opts.cx, &opts.cy, &opts.cw, &opts.ch) != 4) return NGX_ERROR;
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
        if (colon != std::string::npos) {
            std::string k = item.substr(0, colon);
            std::string v = item.substr(colon+1);
            
            if (k == "path") opts.watermark_path = sanitize_path(v);
            else if (k == "o" || k == "opacity") opts.watermark_opacity = std::stod(v);
            else if (k == "x") opts.watermark_x_offset = std::stoi(v);
            else if (k == "y") opts.watermark_y_offset = std::stoi(v);
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
            }
        }
    }
    return NGX_OK;
}

static ngx_int_t parse_single_filter(const std::string& key, const std::string& val, MorphOptions& opts) {
    if (key == "bakground_color" || key == "background_color") { opts.bg_color = val; }
    else if (key == "blur" && !val.empty()) { opts.blur_sigma = std::stod(val); }
    else if (key == "sharpen" && !val.empty()) { opts.sharpen_sigma = std::stod(val); }
    else if (key == "format") { opts.format = val; }
    else if (key == "grayscale") { opts.grayscale = true; }
    else if (key == "quality" && !val.empty()) { opts.quality = std::stoi(val); }
    else if (key == "rotate" && !val.empty()) { opts.rotate_angle = std::stod(val); }
    else if (key == "flip") {
        opts.flip = true;
        opts.flip_dir = (val == "h") ? 1 : 0;
    }
    else if (key == "brightness" && !val.empty()) { opts.brightness = std::stod(val); }
    else if (key == "contrast" && !val.empty()) { opts.contrast = std::stod(val); }
    else if (key == "noise" && !val.empty()) { opts.noise_sigma = std::stod(val); }
    else { return NGX_ERROR; } 
    return NGX_OK;
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
    int w = 0, h = 0;
    if (sscanf(part.c_str(), "%dx%d", &w, &h) != 2) return NGX_ERROR;
    opts.width = w;
    opts.height = h;
    return NGX_OK;
}

/**
 * parse_options
 * @description Parse the URL option segment to extract image processing parameters. / URL 옵션 세그먼트를 파싱하여 이미지 처리 파라미터를 추출합니다.
 */
static ngx_int_t parse_options(const std::string& segment, MorphOptions& opts) {
    size_t start = 0;
    size_t end = segment.find('_');
    
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
        end = segment.find('_', start);
    }
    return NGX_OK;
}

size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *stream) {
    std::vector<char> *data = (std::vector<char> *)stream;
    size_t count = size * nmemb;
    data->insert(data->end(), (char*)ptr, (char*)ptr + count);
    return count;
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
    ctx->status = morph_image_process(&ctx->options, &ctx->output_data, log, &ctx->is_cache_hit, &ctx->last_modified);
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

    ngx_http_send_header(r);
    ngx_http_output_filter(r, &out);
    
    if (ctx->options.debug) {
        std::string client_ip((char*)r->connection->addr_text.data, r->connection->addr_text.len);
        MorphLogger::instance().debug("Status: OK, Client IP: %s", client_ip.c_str());
    }

    delete ctx;
    
    ngx_http_finalize_request(r, NGX_HTTP_OK);
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

    std::string uri((char*)r->uri.data, r->uri.len);

    if (uri.find("/morph/") == 0) {
        uri = uri.substr(7);
    } else if (uri.find("/") == 0) {
        uri = uri.substr(1);
    }
    
    size_t first_slash = uri.find('/');
    size_t second_slash = uri.find('/', first_slash + 1);
    
    if (first_slash == std::string::npos || second_slash == std::string::npos) {
        return NGX_HTTP_BAD_REQUEST;
    }
    
    std::string service_name = uri.substr(0, first_slash);
    std::string options_str = uri.substr(first_slash + 1, second_slash - (first_slash + 1));
    std::string source_path = uri.substr(second_slash + 1); 
    
    ngx_http_core_loc_conf_t *clcf = (ngx_http_core_loc_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    std::string doc_root((char*)clcf->root.data, clcf->root.len);

    MorphOptions options;
    if (parse_options(options_str, options) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Morph: Invalid options format: %s", options_str.c_str());
        return NGX_HTTP_BAD_REQUEST;
    }
    options.source_path = source_path;
    options.service_name = service_name;
    options.document_root = doc_root;
    options.raw_options = options_str;
    
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
    
    if (ctx->options.quality == 0) ctx->options.quality = cf->quality != NGX_CONF_UNSET ? cf->quality : 90;
    ctx->options.debug = cf->debug == 1;

    if (ctx->options.debug) {
        std::string req_url((char*)r->uri.data, r->uri.len);
        MorphLogger::instance().debug("Request URL: %s", req_url.c_str());
    }
    ctx->options.rotate_angle = 0.0;
    ctx->options.brightness = 1.0;
    ctx->options.contrast = 1.0;
    ctx->options.noise_sigma = 0.0;
    ctx->options.flip = false;
    
    if (parse_options(options_str, ctx->options) != NGX_OK) {
        delete ctx;
        return NGX_HTTP_BAD_REQUEST;
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