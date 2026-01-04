#ifndef NGX_HTTP_MORPH_GLOBALS_H
#define NGX_HTTP_MORPH_GLOBALS_H

#include "ngx_http_morph_types.h"

// 전역 설정 맵 (서비스 이름 -> 설정)
extern std::map<std::string, MorphServiceConfig> g_morph_services;

// 설정 파일 경로 (nginx.conf에서 저장됨)
extern std::string g_morph_config_file_path;

#endif // NGX_HTTP_MORPH_GLOBALS_H
