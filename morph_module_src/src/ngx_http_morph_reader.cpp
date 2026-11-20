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
#include "ngx_http_morph_reader.h"
#include "ngx_http_morph_types.h"
#include "ngx_http_morph_globals.h"
#include "ngx_http_morph_utils.h"

// shortcut: fixed cap on downloaded source size, make it a directive if bigger originals are needed
// 다운로드하는 원본 크기의 고정 상한. 더 큰 원본이 필요하면 지시어로 바꾼다
#define MORPH_MAX_SOURCE_BYTES (50 * 1024 * 1024)

// Internal curl write callback
// 내부 curl 쓰기 콜백
static size_t curl_write_to_string(void *ptr, size_t size, size_t nmemb, void *stream) {
    std::string *data = (std::string *)stream;
    size_t count = size * nmemb;
    if (data->size() + count > MORPH_MAX_SOURCE_BYTES) return 0; // aborts the transfer / 전송을 중단시킨다
    data->append((char*)ptr, count);
    return count;
}

// Called for every connection curl makes (incl. redirects), with the already-resolved address,
// so it covers DNS names, numeric tricks (2130706433, 0x7f.1, [::ffff:127.0.0.1]) and rebinding.
// curl이 여는 모든 연결(리다이렉트 포함)마다 이미 해석된 주소로 호출되므로,
// DNS 이름, 숫자 변형(2130706433, 0x7f.1, [::ffff:127.0.0.1]), 리바인딩까지 막는다.
static curl_socket_t curl_open_socket_cb(void *clientp, curlsocktype purpose, struct curl_sockaddr *addr) {
    if (MorphUtils::is_blocked_addr(&addr->addr)) {
        *(bool *)clientp = true;
        return CURL_SOCKET_BAD;
    }
    return socket(addr->family, addr->socktype, addr->protocol);
}

// Read raw image data from a local file or remote URL (with multi-source failover).
// 로컬 파일이나 원격 URL에서 원본 이미지 데이터를 읽는다 (여러 소스 failover 지원).
ngx_int_t morph_reader_read_source(MorphOptions *options, std::string *out_buffer, ngx_log_t *log)
{
    std::string source_path = options->source_path;
    std::vector<std::string> try_urls;
    if (source_path.empty()) return NGX_HTTP_NOT_FOUND;

    // Client-supplied URLs are SSRF-checked; origins from the service config are trusted (may be internal).
    // 클라이언트가 준 URL은 SSRF 검사를 하고, 서비스 설정의 origin은 신뢰한다 (내부망일 수 있다).
    bool user_url = MorphUtils::is_url(source_path);

    if (user_url) {
        if (source_path.find("https:/") == 0 && source_path.find("https://") == std::string::npos) {
            source_path.replace(0, 7, "https://");
        } else if (source_path.find("http:/") == 0 && source_path.find("http://") == std::string::npos) {
            source_path.replace(0, 6, "http://");
        }
        
        try_urls.push_back(MorphUtils::uri_escape(source_path, true));

    } else {
        if (source_path.find("..") != std::string::npos) {
            ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Security detected path traversal attempt: %s", source_path.c_str());
            return NGX_HTTP_FORBIDDEN;
        }
        
        if (g_morph_services.find(options->service_name) != g_morph_services.end()) {
            MorphServiceConfig& svc = g_morph_services[options->service_name];
            
            for (size_t i=0; i<svc.sources.size(); i++) {
                std::string base = svc.sources[i];
                if (!base.empty() && base.back() != '/' && source_path.front() != '/') base += "/";
                // r->uri is decoded: re-encode so "#", "?", "%2e%2e", spaces reach the origin as path bytes
                // r->uri는 디코딩된 값이라 다시 인코딩한다. 그래야 "#", "?", "%2e%2e", 공백이 경로 바이트로 origin에 간다
                try_urls.push_back(base + MorphUtils::uri_escape(source_path));
            }
        }
        
        if (try_urls.empty()) {
            // render lock files live in {root}/{service}/@lock/ and are not sources
            // 렌더 잠금 파일은 {root}/{service}/@lock/에 있고 원본이 아니다
            if (("/" + source_path + "/").find("/@lock/") != std::string::npos) return NGX_HTTP_NOT_FOUND;

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

            if (fsize <= 0 || fsize > MORPH_MAX_SOURCE_BYTES) {
                fclose(fp);
                ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Source empty or too large (%l bytes): %s", fsize, full_path.c_str());
                return NGX_HTTP_NOT_FOUND;
            }

            out_buffer->resize(fsize);
            size_t got = fread(&(*out_buffer)[0], 1, fsize, fp);
            fclose(fp);
            if (got != (size_t)fsize) {
                out_buffer->clear();
                ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Short read: %s", full_path.c_str());
                return NGX_HTTP_NOT_FOUND;
            }
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
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
            bool ssrf_blocked = false;
            if (user_url) {
                curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, curl_open_socket_cb);
                curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &ssrf_blocked);
            }
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // runs in a thread pool thread / 스레드풀 스레드에서 실행됨
            if (user_url) {
                // a proxy would make the socket check see the proxy address instead of the target
                // 프록시를 거치면 소켓 검사가 대상 대신 프록시 주소를 보게 된다
                curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
            }
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); 
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);    
#if LIBCURL_VERSION_NUM >= 0x075500  /* 7.85.0 */
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
            
            CURLcode res = curl_easy_perform(curl);
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            curl_easy_cleanup(curl);

            if (ssrf_blocked && res != CURLE_OK) {
                ngx_log_error(NGX_LOG_ERR, log, 0, "Morph: Security blocked private/internal address: %s", target_url.c_str());
                out_buffer->clear();
                return NGX_HTTP_FORBIDDEN;
            }
            
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

