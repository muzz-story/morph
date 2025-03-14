#include "std.h"

#ifdef __cplusplus
extern "C" {
#endif

static char* ngx_http_morph( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );

static ngx_command_t ngx_http_morph_commands[] = 
{
	{ 
		ngx_string( "morph_thumbnail" ),
		NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
		ngx_http_morph,
		0,
		0,
		NULL
	},	

	ngx_null_command
};

static ngx_http_module_t ngx_http_morph_module_ctx = 
{
	NULL,                                                   /* preconfiguration */
	NULL,                                                   /* postconfiguration */

	NULL,                                                   /* create main configuration */
	NULL,                                                   /* init main configuration */

	NULL,                                                   /* create server configuration */
	NULL,                                                   /* merge server configuration */

	NULL,                                                   /* create location configuration */
	NULL                                                    /* merge location configuration */
};

ngx_module_t ngx_http_morph_module = 
{
	NGX_MODULE_V1,
	&ngx_http_morph_module_ctx,                             /* module context */
	ngx_http_morph_commands,                                /* module directives */
	NGX_HTTP_MODULE,                                        /* module type */
	NULL,                                                   /* init master */
	NULL,                                                   /* init module */
	NULL,                                                   /* init process */
	NULL,                                                   /* init thread */
	NULL,                                                   /* exit thread */
	NULL,                                                   /* exit process */
	NULL,                                                   /* exit master */
	NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_morph_handler( ngx_http_request_t* r )
{
    ngx_chain_t out;
    ngx_buf_t* b;
    
    b = (ngx_buf_t*)ngx_pcalloc( r->pool, sizeof(ngx_buf_t) );
	if( b == NULL )
	{
		ngx_log_error( NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer." );
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	VipsImage *in;

    if( !(in = vips_image_new_from_file( "/svc/morph/nginx/html/input.jpg", NULL)) )
	{
		vips_error_exit(NULL);
	}

	if( 1 )
	{
		char log[ 1024*10 ];
		sprintf(log, "DEBUG : image width = %d, image height = %d\n", vips_image_get_width(in), vips_image_get_height(in) );
		FILE* fp = fopen("/svc/morph/nginx/sbin/morph_debug.log", "a" );
		fwrite( log, 1, strlen(log), fp );
		fclose( fp );
	}  

    out.buf = b;
    out.next = NULL;

    b->pos = (u_char*)"Hello, World!";
    b->last = b->pos + sizeof("Hello, World!") - 1;
    b->memory = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;

    ngx_str_set( &r->headers_out.content_type, "text/plain" );
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = sizeof("Hello, World!") - 1;

    ngx_http_send_header( r );
    return ngx_http_output_filter( r, &out );
}

// Initialize the Morph Modules
static char* ngx_http_morph( ngx_conf_t* cf, ngx_command_t* cmd, void* conf )
{
    ngx_http_core_loc_conf_t* core_location_conf = ( ngx_http_core_loc_conf_t* )ngx_http_conf_get_module_loc_conf( cf, ngx_http_core_module );
    core_location_conf->handler = ngx_http_morph_handler;

	return NGX_CONF_OK;
}

#ifdef __cplusplus
}
#endif