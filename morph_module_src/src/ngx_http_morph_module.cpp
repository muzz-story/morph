#include "std.h"
#include "ngx_http_morph_image.h"
#include <curl/curl.h>

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
} ngx_http_morph_loc_conf_t;

// Image Process Options / 이미지 처리 옵션
// MorphOptions is defined in ngx_http_morph_image.h

/**
 * ngx_http_morph
 * @description Initialize the module and link handler. / 모듈을 초기화하고 핸들러를 연결합니다.
 * @param {ngx_conf_t*} cf - Configuration context. / 설정 컨텍스트.
 * @param {ngx_command_t*} cmd - Command structure. / 명령어 구조체.
 * @param {void*} conf - Custom configuration. / 사용자 정의 설정.
 * @returns {char*} - NGX_CONF_OK. / 성공 시 NGX_CONF_OK 반환.
 */
static char* ngx_http_morph( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );
/**
 * ngx_http_morph_create_loc_conf
 * @description Create location configuration structure. / Location 설정 구조체를 생성합니다.
 * @param {ngx_conf_t*} cf - Configuration context. / 설정 컨텍스트.
 * @returns {void*} - Created configuration structure. / 생성된 설정 구조체.
 */
static void* ngx_http_morph_create_loc_conf(ngx_conf_t *cf);
/**
 * ngx_http_morph_merge_loc_conf
 * @description Merge location configurations. / Location 설정을 병합합니다.
 * @param {ngx_conf_t*} cf - Configuration context. / 설정 컨텍스트.
 * @param {void*} parent - Parent configuration. / 부모 설정.
 * @param {void*} child - Child configuration. / 자식 설정.
 * @returns {char*} - NGX_CONF_OK or error. / 성공 시 NGX_CONF_OK 반환.
 */
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

ngx_module_t ngx_http_morph_module = 
{
    NGX_MODULE_V1,
    &ngx_http_morph_module_ctx,                             /* module context */
    ngx_http_morph_commands,                                /* module directives */
    NGX_HTTP_MODULE,                                        /* module type */
    NULL,                                                   /* init master */
    NULL,                                                   /* init module */
    NULL,                                                   /* init process */
    NULL,                                                   /* init thread */
    NULL,                                                   /* exit thread */
    NULL,                                                   /* exit process */
    NULL,                                                   /* exit master */
    NGX_MODULE_V1_PADDING
};

/**
 * ngx_http_morph_create_loc_conf
 * @description Create location configuration structure. / Location 설정 구조체를 생성합니다.
 * @param {ngx_conf_t*} cf - Configuration context. / 설정 컨텍스트.
 * @returns {void*} - Created configuration structure. / 생성된 설정 구조체.
 */
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
    // ngx_str_t is initialized to {0, NULL} by pcalloc

    return conf;
}

/**
 * ngx_http_morph_merge_loc_conf
 * @description Merge location configurations. / Location 설정을 병합합니다.
 * @param {ngx_conf_t*} cf - Configuration context. / 설정 컨텍스트.
 * @param {void*} parent - Parent configuration. / 부모 설정.
 * @param {void*} child - Child configuration. / 자식 설정.
 * @returns {char*} - NGX_CONF_OK or error. / 성공 시 NGX_CONF_OK 반환.
 */
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

    return NGX_CONF_OK;
}

/**
 * parse_options
 * @description Parse the URL option segment to extract image processing parameters. / URL 옵션 세그먼트를 파싱하여 이미지 처리 파라미터를 추출합니다.
 * @param {const std::string&} segment - The option segment string (e.g., "200x300_C10,10,10,20"). / 옵션 문자열.
 * @param {MorphOptions&} opts - The structure to store parsed options. / 파싱된 옵션을 저장할 구조체.
 * @returns {void}
 */
static void parse_options(const std::string& segment, MorphOptions& opts) {
    // 200x300
    // 200x300_C10,10,10,20
    // 200x300_F(bakground_color:ff00ff)
    
    // Split by '_'
    std::regex underscore_re("_");
    std::sregex_token_iterator it(segment.begin(), segment.end(), underscore_re, -1);
    std::sregex_token_iterator end;

    for (; it != end; ++it) {
        std::string part = *it;
        
        // Dimension: 200x300
        if (std::regex_match(part, std::regex("^\\d+x\\d+$"))) {
            sscanf(part.c_str(), "%dx%d", &opts.width, &opts.height);
            continue;
        }

        // Crop: C10,10,10,20
        if (part[0] == 'C') {
            opts.has_crop = true;
            sscanf(part.c_str(), "C%d,%d,%d,%d", &opts.cx, &opts.cy, &opts.cw, &opts.ch);
            continue;
        }

        // Filters: F(...)
        if (part[0] == 'F' && part.size() > 2 && part[1] == '(' && part.back() == ')') {
            std::string content = part.substr(2, part.size() - 3); // remove F( and )
            
            // content might be "key:value" or "key"
            size_t colon_pos = content.find(':');
            std::string key = (colon_pos == std::string::npos) ? content : content.substr(0, colon_pos);
            std::string val = (colon_pos == std::string::npos) ? "" : content.substr(colon_pos + 1);

            if (key == "bakground_color" || key == "background_color") opts.bg_color = val;
            else if (key == "blur") opts.blur_sigma = std::stod(val);
            else if (key == "format") opts.format = val;
            else if (key == "grayscale") opts.grayscale = true;
            else if (key == "quality") opts.quality = std::stoi(val);
            else if (key == "rotate") opts.rotate_angle = std::stod(val);
            else if (key == "flip") {
                opts.flip = true;
                opts.flip_dir = (val == "h") ? 1 : 0;
            }
            else if (key == "brightness") opts.brightness = std::stod(val);
            else if (key == "contrast") opts.contrast = std::stod(val);
            else if (key == "noise") opts.noise_sigma = std::stod(val);
        }
    }
}

/**
 * curl_write_cb
 * @description Callback function for libcurl to write received data into a vector. / Libcurl이 데이터를 수신했을 때 벡터에 저장하기 위한 콜백 함수입니다.
 * @param {void*} ptr - Pointer to the received data. / 수신된 데이터 포인터.
 * @param {size_t} size - Size of one data element. / 데이터 요소의 크기.
 * @param {size_t} nmemb - Number of elements. / 요소의 개수.
 * @param {void*} stream - User-defined stream pointer (std::vector<char>*). / 사용자 정의 스트림 포인터.
 * @returns {size_t} - Number of bytes processed. / 처리된 바이트 수.
 */
size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *stream) {
    std::vector<char> *data = (std::vector<char> *)stream;
    size_t count = size * nmemb;
    data->insert(data->end(), (char*)ptr, (char*)ptr + count);
    return count;
}

// Thread Context Structure to handle C++ objects
struct MorphThreadCtx {
    MorphOptions options;
    std::string output_data;
    ngx_http_request_t *r;
    ngx_int_t status;
    
    MorphThreadCtx() : r(NULL), status(NGX_OK) {}
};

// Thread Worker Function
static void morph_thread_func(void *data, ngx_log_t *log)
{
    MorphThreadCtx *ctx = (MorphThreadCtx *)data;
    ctx->status = morph_image_process(&ctx->options, &ctx->output_data, log);
}

// Thread Completion Function
static void morph_thread_completion(ngx_event_t *ev)
{
    ngx_thread_task_t *task = (ngx_thread_task_t *)ev->data;
    MorphThreadCtx *ctx = (MorphThreadCtx *)task->ctx;
    ngx_http_request_t *r = ctx->r;
    
    if (ctx->status != NGX_OK) {
        // Error handling
        ngx_http_finalize_request(r, ctx->status);
        delete ctx;
        return;
    }

    // Success - Send Response
    // Copy output_data to Nginx pool buffer
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

    // Set Content-Type based on format or magic number?
    // Options has format.
    std::string fmt = ctx->options.format;
    if (fmt.empty()) fmt = "jpg"; // default

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

    ngx_http_send_header(r);
    ngx_http_output_filter(r, &out);
    
    // Cleanup C++ context
    delete ctx;
    
    ngx_http_finalize_request(r, NGX_HTTP_OK);
}

/**
 * ngx_http_morph_handler
 * @description Main request handler to process image transformation requests. / 이미지 변환 요청을 처리하는 메인 핸들러입니다.
 * @param {ngx_http_request_t*} r - The Nginx request structure. / Nginx 요청 구조체.
 * @returns {ngx_int_t} - HTTP status code or Nginx return code. / HTTP 상태 코드 또는 Nginx 반환 코드.
 */
static ngx_int_t ngx_http_morph_handler( ngx_http_request_t* r )
{
    ngx_http_morph_loc_conf_t *cf;
    cf = (ngx_http_morph_loc_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_morph_module);

    if (cf->enable == 0) {
        return NGX_DECLINED;
    }

    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    // URI Parsing
    std::string uri((char*)r->uri.data, r->uri.len);
    
    // Check Prefix (Assuming /morph/ or /)
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
    
    // Get Root Path
    ngx_http_core_loc_conf_t *clcf = (ngx_http_core_loc_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    std::string doc_root((char*)clcf->root.data, clcf->root.len);

    // Prepare Thread Context
    MorphThreadCtx *ctx = new MorphThreadCtx();
    ctx->r = r;
    ctx->options.quality = cf->quality != NGX_CONF_UNSET ? cf->quality : 90;
    ctx->options.source_path = source_path;
    ctx->options.service_name = service_name;
    ctx->options.document_root = doc_root;
    
    // Default Filters
    ctx->options.brightness = 1.0;
    ctx->options.contrast = 1.0;
    ctx->options.noise_sigma = 0.0;
    ctx->options.flip = false;
    
    parse_options(options_str, ctx->options);

    // Create Thread Task
    ngx_thread_task_t *task = ngx_thread_task_alloc(r->pool, 0);
    if (task == NULL) {
        delete ctx;
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    task->handler = morph_thread_func;
    task->ctx = ctx;
    task->event.handler = morph_thread_completion;
    task->event.data = task;

    // Post to default pool (NULL)
    if (ngx_thread_task_post(NULL, task) != NGX_OK) {
        delete ctx;
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // Increment request count to prevent cleanup while thread runs
    r->main->count++;

    return NGX_DONE; // Async processing started
}

/**
 * ngx_http_morph
 * @description Initialize the module and link handler. / 모듈을 초기화하고 핸들러를 연결합니다.
 * @param {ngx_conf_t*} cf - Configuration context. / 설정 컨텍스트.
 * @param {ngx_command_t*} cmd - Command structure. / 명령어 구조체.
 * @param {void*} conf - Custom configuration. / 사용자 정의 설정.
 * @returns {char*} - NGX_CONF_OK. / 성공 시 NGX_CONF_OK 반환.
 */
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