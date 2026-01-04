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

}

#endif // NGX_HTTP_MORPH_UTILS_H
