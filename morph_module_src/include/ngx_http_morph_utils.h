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

#ifndef NGX_HTTP_MORPH_UTILS_H
#define NGX_HTTP_MORPH_UTILS_H

#include "std.h"

class MorphLogger {
public:
    static MorphLogger& instance();
    bool open(const std::string& path);
    void close();
    void debug(const char* fmt, ...);
private:
    MorphLogger() {}
    ~MorphLogger();
    
    std::ofstream log_file;
    std::mutex log_mutex;
};

namespace MorphUtils {
    std::string compute_md5(const std::string& str);
    int ensure_directory(const std::string& path);
    std::string sanitize_path(const std::string& str);
    // Injective file-name encoding: [A-Za-z0-9._-] kept, everything else %XX. Different inputs never
    // share a cache directory (sanitize_path maps "(" ")" "," ":" all to "_").
    // 일대일 파일명 인코딩: [A-Za-z0-9._-]는 그대로, 나머지는 %XX. 입력이 다르면 캐시 디렉터리도
    // 겹치지 않는다 (sanitize_path는 "(" ")" "," ":"를 모두 "_"로 바꿔서 겹쳤다).
    std::string escape_component(const std::string& str);
    // Percent-encode a decoded URI path for re-sending upstream ("#", "?", "%", space, ... encoded).
    // client_url keeps "?" "&" "=" "%" literal: a client URL is fully client-controlled and may carry an
    // already-encoded query (sent as %3F...). Paths appended to service origins get everything encoded.
    // 디코딩된 URI 경로를 upstream에 다시 보낼 때 퍼센트 인코딩한다 ("#", "?", "%", 공백 등).
    // client_url이면 "?" "&" "=" "%"는 그대로 둔다: 클라이언트 URL은 전부 클라이언트가 정하고 이미
    // 인코딩된 쿼리(%3F...)를 담을 수 있다. 서비스 origin에 붙이는 경로는 모두 인코딩한다.
    std::string uri_escape(const std::string& str, bool client_url = false);
    // Source path is a remote URL ("http:/" or "https:/"; nginx merges the double slash).
    // 원본 경로가 원격 URL인지 ("http:/" 또는 "https:/"; nginx가 이중 슬래시를 하나로 합친다).
    bool is_url(const std::string& path);
    // True for loopback, private, link-local, CGNAT, multicast, reserved (IPv4 and IPv6, incl. v4-mapped).
    // loopback, 사설, link-local, CGNAT, 멀티캐스트, 예약 대역이면 true (IPv4/IPv6, v4-mapped 포함).
    bool is_blocked_addr(const struct sockaddr *sa);

}

#endif // NGX_HTTP_MORPH_UTILS_H
