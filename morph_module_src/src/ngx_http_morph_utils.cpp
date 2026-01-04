#include "std.h"
#include "ngx_http_morph_utils.h"

// ==========================================
// MorphLogger 구현 (Implementation)
// ==========================================

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
    
    // Append 모드로 열기
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

    // 현재 시간 포맷팅
    std::time_t now = std::time(nullptr);
    char time_buf[32];
    std::strftime(time_buf, sizeof(time_buf), "%Y/%m/%d %H:%M:%S", std::localtime(&now));

    // 메시지 포맷팅
    char msg_buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    // 파일 쓰기
    log_file << time_buf << " [Debug] " << msg_buf << std::endl;
}

// ==========================================
// MorphUtils 구현 (Implementation)
// ==========================================

namespace MorphUtils {

std::string compute_md5(const std::string& str) {
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)str.c_str(), str.size(), result);

    std::stringstream ss;
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];
    }
    return ss.str();
}

int ensure_directory(const std::string& path) {
    std::string current_path;
    std::string rest = path;
    
    // 절대 경로 처리
    if (path.length() > 0 && path[0] == '/') {
        current_path = "/";
        rest = path.substr(1);
    }
    
    size_t pos = 0;
    while((pos = rest.find('/')) != std::string::npos) {
        current_path += rest.substr(0, pos);
        
        struct stat st;
        if (stat(current_path.c_str(), &st) != 0) {
            // 디렉토리가 없으면 생성 (권한 0755)
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
