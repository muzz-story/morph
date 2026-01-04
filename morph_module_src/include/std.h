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