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

#ifndef STD_H
#define STD_H

#include <fcntl.h>
#include <stdio.h>

#include <string>
#include <map>
#include <list>
#include <math.h>

#include <vector>
#include <mutex>
#include <curl/curl.h>
#include <cstdarg>
#include <openssl/md5.h>
#include <ctime>
#include <iomanip>

#define __max(a,b) (((a) > (b)) ? (a) : (b)) 
#define __min(a,b) (((a) < (b)) ? (a) : (b)) 

extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_thread_pool.h>
}

#include <vips/vips8>

// JSON & Streams
#include <fstream>
#include <iostream>
#include <sstream>
#include "json.hpp"
using json = nlohmann::json;

#endif // STD_H