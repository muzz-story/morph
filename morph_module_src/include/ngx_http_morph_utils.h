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

}

#endif // NGX_HTTP_MORPH_UTILS_H
