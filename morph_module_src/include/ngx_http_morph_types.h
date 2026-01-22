#ifndef NGX_HTTP_MORPH_TYPES_H
#define NGX_HTTP_MORPH_TYPES_H

#include "std.h"

// Gravity Constants
#define MORPH_GRAVITY_CENTER 0
#define MORPH_GRAVITY_TOP    1
#define MORPH_GRAVITY_BOTTOM 2
#define MORPH_GRAVITY_LEFT   3
#define MORPH_GRAVITY_RIGHT  4
#define MORPH_GRAVITY_TOP_LEFT 5
#define MORPH_GRAVITY_TOP_RIGHT 6
#define MORPH_GRAVITY_BOTTOM_LEFT 7
#define MORPH_GRAVITY_BOTTOM_RIGHT 8

// 이미지 처리 옵션 (Image Process Options)
typedef struct {
    int width = 0;
    int height = 0;
    bool debug = false;
    
    // Crop Settings
    bool has_crop = false; // Manual Crop (Cx, Cy, Cw, Ch)
    int cx = 0;
    int cy = 0;
    int cw = 0;
    int ch = 0;

    // Smart Crop / Resize Settings
    int gravity = MORPH_GRAVITY_CENTER; // For "Cover" resize strategy if no manual crop

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

    // Sharpen Filter
    double sharpen_sigma = 0.0;

    // Watermark
    std::string watermark_path;
    int watermark_gravity = MORPH_GRAVITY_CENTER;
    double watermark_opacity = 1.0;
    int watermark_x_offset = 0;
    int watermark_y_offset = 0;
    
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
