#ifndef NGX_HTTP_MORPH_IMAGE_H
#define NGX_HTTP_MORPH_IMAGE_H

#include "std.h"

// Forward declaration or include common structs
// Assuming MorphOptions is defined in a common place or passed as void* for now to keep headers clean,
// but better to include the struct definition from a common header.
// For this stub, I will assume a struct MorphOptions is passed.

// MorphOptions definition moved to std.h

typedef enum {
    MORPH_IMG_UNKNOWN = 0,
    MORPH_IMG_JPEG,
    MORPH_IMG_PNG,
    MORPH_IMG_GIF,
    MORPH_IMG_WEBP
} MorphImageType;

/**
 * morph_image_process
 * @description Orchestrate the entire image processing flow: Read -> Check Hex -> Resize -> Filter -> Output. / 전체 이미지 처리 흐름을 관리합니다: 읽기 -> Hex 확인 -> 리사이즈 -> 필터 -> 출력.
 * @param {MorphOptions*} options - MorphOptions structure pointer. / MorphOptions 구조체 포인터.
 * @param {std::string*} out_data - Output image data buffer. / 출력 이미지 데이터 버퍼.
 * @param {ngx_log_t*} log - Logger. / 로거.
 * @returns {ngx_int_t} - NGX_OK or error. / 성공 시 NGX_OK 또는 에러.
 */
ngx_int_t morph_image_process(MorphOptions *options, std::string *out_data, ngx_log_t *log);

/**
 * morph_image_validate_hex
 * @description Validate file magic numbers and identify format. / 파일 매직 넘버를 검사하여 유효성을 확인하고 포맷을 식별합니다.
 * @param {void*} data - Pointer to file data. / 파일 데이터 포인터.
 * @param {size_t} len - Data length. / 데이터 길이.
 * @param {int*} out_type - Identified image type (MorphImageType). / 식별된 이미지 타입.
 * @returns {ngx_int_t} - NGX_OK (valid) or NGX_ERROR (invalid). / 유효하면 NGX_OK, 아니면 NGX_ERROR.
 */
ngx_int_t morph_image_validate_hex(void *data, size_t len, int *out_type);

/**
 * morph_image_get_cache_path
 * @description Calculate local cache file path based on options. / 옵션에 기반하여 로컬 캐시 파일 경로를 계산합니다.
 * @param {MorphOptions*} options - MorphOptions structure pointer. / MorphOptions 구조체 포인터.
 * @returns {std::string} - Absolute path to cache file. / 캐시 파일 절대 경로.
 */
std::string morph_image_get_cache_path(MorphOptions *options);

#endif