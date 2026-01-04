#ifndef NGX_HTTP_MORPH_READER_H
#define NGX_HTTP_MORPH_READER_H

#include "ngx_http_morph_types.h"

/**
 * morph_reader_read_source
 * @description Read raw image data from local or remote source. / 로컬 또는 원격 소스에서 원시 이미지 데이터를 읽어옵니다.
 * @param {MorphOptions*} options - Image options containing path info. / 경로 정보를 포함한 이미지 옵션.
 * @param {std::string*} out_buffer - Buffer to store read data. / 읽은 데이터를 저장할 버퍼.
 * @param {ngx_log_t*} log - Logger. / 로거.
 * @returns {ngx_int_t} - NGX_OK or error code. / 성공 시 NGX_OK 또는 에러 코드.
 */
ngx_int_t morph_reader_read_source(MorphOptions *options, std::string *out_buffer, ngx_log_t *log);

/**
 * morph_reader_check_cache
 * @description Check if the image exists in cache. / 이미지가 캐시에 존재하는지 확인합니다(추후 구현).
 * @param {const char*} path - Image path. / 이미지 경로.
 * @returns {ngx_int_t} - NGX_OK (hit) or NGX_DECLINED (miss). / 캐시 히트 시 NGX_OK, 미스 시 NGX_DECLINED.
 */
ngx_int_t morph_reader_check_cache(const char *path);

#endif