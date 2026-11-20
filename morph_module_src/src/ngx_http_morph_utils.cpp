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
#include "ngx_http_morph_utils.h"

#include <netinet/in.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/evp.h>
#endif

MorphLogger& MorphLogger::instance() {
    static MorphLogger instance;
    return instance;
}

MorphLogger::~MorphLogger() {
    close();
}

bool MorphLogger::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_file.is_open()) {
        log_file.close();
    }
    
    log_file.open(path.c_str(), std::ios::app);
    return log_file.is_open();
}

void MorphLogger::close() {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_file.is_open()) {
        log_file.close();
    }
}

void MorphLogger::debug(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    std::time_t now = std::time(nullptr);
    char time_buf[32];
    std::strftime(time_buf, sizeof(time_buf), "%Y/%m/%d %H:%M:%S", std::localtime(&now));

    char msg_buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    log_file << time_buf << " [Debug] " << msg_buf << std::endl;
}

namespace MorphUtils {

std::string compute_md5(const std::string& str) {
    unsigned char result[MD5_DIGEST_LENGTH];

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    // OpenSSL 3.0+: use EVP API (MD5() is deprecated)
    // OpenSSL 3.0+: EVP API 사용 (MD5()는 deprecated)
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    EVP_DigestUpdate(ctx, str.c_str(), str.size());
    unsigned int md_len = 0;
    EVP_DigestFinal_ex(ctx, result, &md_len);
    EVP_MD_CTX_free(ctx);
#else
    MD5((unsigned char*)str.c_str(), str.size(), result);
#endif

    std::stringstream ss;
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];
    }
    return ss.str();
}

int ensure_directory(const std::string& path) {
    std::string current_path;
    std::string rest = path;
    
    if (path.length() > 0 && path[0] == '/') {
        current_path = "/";
        rest = path.substr(1);
    }
    
    size_t pos = 0;
    while((pos = rest.find('/')) != std::string::npos) {
        current_path += rest.substr(0, pos);
        
        struct stat st;
        if (stat(current_path.c_str(), &st) != 0) {
            if (mkdir(current_path.c_str(), 0755) != 0 && errno != EEXIST) {
                return -1;
            }
        }
        
        current_path += "/";
        rest = rest.substr(pos + 1);
    }
    return 0;
}

std::string sanitize_path(const std::string& str) {
    std::string safe = str;
    for (size_t i = 0; i < safe.size(); ++i) {
        char c = safe[i];
        if (!isalnum(c) && c != '-' && c != '_' && c != '.') {
            safe[i] = '_';
        }
    }
    return safe;
}

static std::string percent_encode(const std::string& str, const char *keep) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : str) {
        if (isalnum(c) || strchr(keep, c)) {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xf];
        }
    }
    return out;
}

std::string escape_component(const std::string& str) {
    return percent_encode(str, "._-");
}

std::string uri_escape(const std::string& str, bool client_url) {
    // RFC 3986 unreserved + path sub-delims; '[' ']' keep IPv6 literal hosts intact in client URLs
    // RFC 3986 unreserved + 경로 sub-delims. '[' ']'는 클라이언트 URL의 IPv6 리터럴 호스트를 유지한다
    return percent_encode(str, client_url ? "-._~/!$&'()*+,;=:@[]?%" : "-._~/!$'()*+,;:@[]");
}

bool is_url(const std::string& path) {
    return path.compare(0, 6, "http:/") == 0 || path.compare(0, 7, "https:/") == 0;
}

// ip in host byte order
// ip는 호스트 바이트 순서
static bool is_blocked_ipv4(uint32_t ip) {
    uint32_t a = ip >> 24, b = (ip >> 16) & 0xff, c = (ip >> 8) & 0xff;
    return a == 0 || a == 10 || a == 127 || a >= 224 ||   // this-net, RFC1918, loopback, multicast/reserved / this-net, 사설망, loopback, 멀티캐스트/예약
           (a == 100 && (b & 0xc0) == 64) ||              // 100.64.0.0/10 CGNAT
           (a == 169 && b == 254) ||                      // link-local, cloud metadata / 클라우드 메타데이터
           (a == 172 && (b & 0xf0) == 16) ||              // 172.16.0.0/12 (Docker bridges) / Docker 브리지
           (a == 192 && b == 168) ||
           (a == 192 && b == 0 && c == 0) ||              // 192.0.0.0/24
           (a == 198 && (b & 0xfe) == 18);                // 198.18.0.0/15 benchmarking / 벤치마크용
}

bool is_blocked_addr(const struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return is_blocked_ipv4(ntohl(((const struct sockaddr_in *)sa)->sin_addr.s_addr));
    }
    if (sa->sa_family == AF_INET6) {
        const struct in6_addr *a6 = &((const struct sockaddr_in6 *)sa)->sin6_addr;
        const uint8_t *b = a6->s6_addr;
        static const uint8_t nat64[12] = {0x00, 0x64, 0xff, 0x9b};                 // 64:ff9b::/96
        static const uint8_t v4xlat[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0, 0}; // ::ffff:0:0/96
        uint32_t v4;
        if (IN6_IS_ADDR_V4MAPPED(a6) || IN6_IS_ADDR_V4COMPAT(a6) ||
            memcmp(b, nat64, 12) == 0 || memcmp(b, v4xlat, 12) == 0) {
            memcpy(&v4, b + 12, 4);
            return is_blocked_ipv4(ntohl(v4));
        }
        if (b[0] == 0x20 && b[1] == 0x02) {                  // 2002::/16 6to4 embeds IPv4 in bytes 2-5 / 6to4는 2-5바이트에 IPv4를 담는다
            memcpy(&v4, b + 2, 4);
            return is_blocked_ipv4(ntohl(v4));
        }
        return IN6_IS_ADDR_UNSPECIFIED(a6) || IN6_IS_ADDR_LOOPBACK(a6) ||
               (b[0] & 0xfe) == 0xfc ||                      // fc00::/7 ULA
               (b[0] == 0xfe && (b[1] & 0xc0) == 0x80) ||    // fe80::/10 link-local
               (b[0] == 0xfe && (b[1] & 0xc0) == 0xc0) ||    // fec0::/10 site-local (deprecated) / site-local (폐기됨)
               (b[0] == 0x00 && b[1] == 0x64 && b[2] == 0xff && b[3] == 0x9b && b[4] == 0x00 && b[5] == 0x01) || // 64:ff9b:1::/48 local NAT64 / 로컬 NAT64
               (b[0] == 0x20 && b[1] == 0x01 && b[2] == 0x00 && b[3] == 0x00) ||  // 2001::/32 Teredo
               b[0] == 0xff;                                 // multicast / 멀티캐스트
    }
    return true; // unknown family: refuse / 알 수 없는 주소 체계: 거부
}

} // namespace MorphUtils
