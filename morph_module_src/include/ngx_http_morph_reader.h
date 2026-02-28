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

#ifndef NGX_HTTP_MORPH_READER_H
#define NGX_HTTP_MORPH_READER_H

#include "ngx_http_morph_types.h"

// Read raw image data from a local file or remote URL (with multi-source failover).
ngx_int_t morph_reader_read_source(MorphOptions *options, std::string *out_buffer, ngx_log_t *log);

#endif