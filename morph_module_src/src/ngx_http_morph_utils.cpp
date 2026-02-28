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

} // namespace MorphUtils
