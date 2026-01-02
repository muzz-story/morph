#ifndef STD_H
#define STD_H

#include <fcntl.h>
#include <stdio.h>

#include <string>
#include <map>
#include <list>
#include <math.h>

#include <vector>

#define __max(a,b) (((a) > (b)) ? (a) : (b)) 
#define __min(a,b) (((a) < (b)) ? (a) : (b)) 

extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_thread_pool.h>
}

#include <vips/vips8>

// Image Process Options / 이미지 처리 옵션
typedef struct {
    int width;
    int height;
    bool debug;
    
    // Crop
    int cw, ch, cx, cy;
    bool has_crop;

    // Filters
    std::string bg_color;
    double blur_sigma;
    std::string format;
    bool grayscale;
    int quality;
    double rotate_angle;
    
    // New Filters
    bool flip;
    int flip_dir; // 0: vertical, 1: horizontal
    double brightness;
    double contrast;
    double noise_sigma;
    int noise_type;
    
    // Source
    std::string source_path;
    
    // Path Mapping
    std::string service_name;
    std::string document_root;
    std::string raw_options;
} MorphOptions;
    
#endif//STD_H