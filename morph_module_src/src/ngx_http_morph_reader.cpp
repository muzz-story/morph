#include "std.h"
#include "ngx_http_morph_reader.h"
#include "ngx_http_morph_types.h"
#include "ngx_http_morph_globals.h"
#include "ngx_http_morph_utils.h"

// Internal curl write callback
static size_t curl_write_to_string(void *ptr, size_t size, size_t nmemb, void *stream) {
    std::string *data = (std::string *)stream;
    size_t count = size * nmemb;
    data->append((char*)ptr, count);
    return count;
}

// Header callback to get Content-Length
static size_t curl_header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t numbytes = size * nitems;
    std::string *out_buffer = (std::string *)userdata;

    std::string line(buffer, numbytes);
    if (line.find("Content-Length:") == 0) {
        size_t pos = line.find_first_of("0123456789");
        if (pos != std::string::npos) {
            long len = std::stol(line.substr(pos));
            if (len > 0) {
                out_buffer->reserve(len);
            }
        }
    }
    return numbytes;
}

/**
 * morph_reader_read_source
 * @description 로컬 또는 원격 소스에서 원시 이미지 데이터를 읽어옵니다.
 * @param {MorphOptions*} options - 경로 정보를 포함한 이미지 옵션.
 * @param {std::string*} out_buffer - 읽은 데이터를 저장할 버퍼.
 * @param {ngx_log_t*} log - 로거.
 * @returns {ngx_int_t} - 성공 시 NGX_OK 또는 에러 코드.
 */
ngx_int_t morph_reader_read_source(MorphOptions *options, std::string *out_buffer, ngx_log_t *log)
{
    std::string source_path = options->source_path;
    std::vector<std::string> try_urls;

    if (source_path.find("http") == 0) {
        if (source_path.find("localhost") != std::string::npos ||
            source_path.find("127.") != std::string::npos ||
            source_path.find("192.168.") != std::string::npos ||
            source_path.find("10.") != std::string::npos ||
            source_path.find("::1") != std::string::npos) {
            ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Security blocked suspicious URL: %s", source_path.c_str());
            return NGX_HTTP_FORBIDDEN;
        }

        if (source_path.find("https:/") == 0 && source_path.find("https://") == std::string::npos) {
            source_path.replace(0, 7, "https://");
        } else if (source_path.find("http:/") == 0 && source_path.find("http://") == std::string::npos) {
            source_path.replace(0, 6, "http://");
        }
        
        try_urls.push_back(source_path);

    } else {
        if (source_path.find("..") != std::string::npos) {
            ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Security detected path traversal attempt: %s", source_path.c_str());
            return NGX_HTTP_FORBIDDEN;
        }
        
        if (g_morph_services.find(options->service_name) != g_morph_services.end()) {
            MorphServiceConfig& svc = g_morph_services[options->service_name];
            
            for (size_t i=0; i<svc.sources.size(); i++) {
                std::string base = svc.sources[i];
                if (base.back() != '/' && source_path.front() != '/') base += "/";
                try_urls.push_back(base + source_path);
            }
        }
        
        if (try_urls.empty()) {
             std::string full_path = options->document_root;
            if (!full_path.empty() && full_path.back() != '/') full_path += "/";
            full_path += options->service_name + "/" + source_path;
            
            if (options->debug) {
                 MorphLogger::instance().debug("Source Type: Local Read - %s", full_path.c_str());
            }

            FILE *fp = fopen(full_path.c_str(), "rb");
            if (!fp) {
                ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: File not found: %s", full_path.c_str());
                return NGX_HTTP_NOT_FOUND;
            }

            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (fsize > 0) {
                out_buffer->resize(fsize);
                fread(&(*out_buffer)[0], 1, fsize, fp);
            }
            fclose(fp);
            return NGX_OK;
        }
    }

    for (size_t i=0; i<try_urls.size(); i++) {
        std::string target_url = try_urls[i];
        
        if (options->debug) {
            MorphLogger::instance().debug("Source Type: URL Fetch (Attempt %d/%d) - %s", i+1, try_urls.size(), target_url.c_str());
        }

        CURL *curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, target_url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_to_string);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_buffer);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, out_buffer);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); 
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);    
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
            
            CURLcode res = curl_easy_perform(curl);
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            curl_easy_cleanup(curl);
            
            if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
                if (options->debug) {
                    MorphLogger::instance().debug("Download Success: %lu bytes (from %s)", out_buffer->size(), target_url.c_str());
                }
                return NGX_OK; 
            } else {
                 if (options->debug) {
                    MorphLogger::instance().debug("Download Failed (Code: %d, HTTP: %ld). Trying next...", res, http_code);
                }
                out_buffer->clear();
            }
        }
    }

    return NGX_HTTP_NOT_FOUND; 
}

/**
 * morph_reader_check_cache
 * @description Check if the image exists in cache. / 이미지가 캐시에 존재하는지 확인합니다(추후 구현).
 * @param {const char*} path - Image path. / 이미지 경로.
 * @returns {ngx_int_t} - NGX_OK (hit) or NGX_DECLINED (miss). / 캐시 히트 시 NGX_OK, 미스 시 NGX_DECLINED.
 */
ngx_int_t morph_reader_check_cache(const char *path)
{
    return NGX_DECLINED; 
}