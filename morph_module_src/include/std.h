#ifndef STD_H
#define STD_H

#include <fcntl.h>
#include <stdio.h>

#include <string>
#include <map>
#include <list>
#include <math.h>

#include <vector>
#include <regex>

#define __max(a,b) (((a) > (b)) ? (a) : (b)) 
#define __min(a,b) (((a) < (b)) ? (a) : (b)) 

extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_thread_pool.h>
#include <vips/vips.h>
}
    
#endif//STD_H