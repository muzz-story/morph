#ifndef NGX_HTTP_MORPH_TYPES_H
#define NGX_HTTP_MORPH_TYPES_H

#include "std.h"

typedef struct {
    int width = 0;
    int height = 0;
    bool debug = false;
    
    // 크롭 (Crop)
    int cw = 0, ch = 0, cx = 0, cy = 0;
    bool has_crop = false;

    // 필터 (Filters)
    std::string bg_color;
    double blur_sigma = 0.0;
    std::string format;
    bool grayscale = false;
    int quality = 0;
    double rotate_angle = 0.0;
    
    // 추가 필터 (New Filters)
    bool flip = false;
    int flip_dir = 0; // 0: 수직(vertical), 1: 수평(horizontal)
    double brightness = 1.0;
    double contrast = 1.0;
    double noise_sigma = 0.0;
    int noise_type = 0;
    
    // 원본 경로 (Source)
    std::string source_path;
    
    // 경로 매핑 (Path Mapping)
    std::string service_name;
    std::string document_root;
    std::string raw_options;
} MorphOptions;

struct MorphServiceConfig {
    std::vector<std::string> sources;
    int ttl; // 초 단위, -1이면 무제한 (seconds, -1 for infinite)
};

#endif // NGX_HTTP_MORPH_TYPES_H
