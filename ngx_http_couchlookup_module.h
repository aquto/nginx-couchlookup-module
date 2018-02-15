#ifndef NGX_HTTP_COUCHLOOKUP_MODULE_H
# define NGX_HTTP_COUCHLOOKUP_MODULE_H

# include <libcouchbase/couchbase.h>
# include "ngx_http_hashtb.h"

/**
 * @brief Macros to handle credentials file parsing
 */
# define _CREDS_TOKEN_DELIM (":\n")

# define _HANDLE_CREDS_TOKEN_ERROR(token)                      \
    if (token == NULL)                                         \
    {                                                          \
        ngx_log_stderr(0, "Could not read key "#token". "      \
            "Creds syntax: `HOST:BUCKET:USERNAME:PASSWORD`."); \
        goto failure;                                          \
    }

// First call to strtok, passing in the char ptr
# define SET_FIRST_CREDS_TOK(token, str)      \
    token = strtok(str, _CREDS_TOKEN_DELIM);  \
    _HANDLE_CREDS_TOKEN_ERROR(token)

// Subsequent calls to strtok
# define SET_NEXT_CREDS_TOK(token)            \
    token = strtok(NULL, _CREDS_TOKEN_DELIM); \
    _HANDLE_CREDS_TOKEN_ERROR(token)

/**
 * @brief Macros related to variables
 */
# define VAR_NAME_TPL ("cl_%s")
# define VAR_HTB_SIZE (64)

/**
 * @brief Macros related to JSON parsing
 */
# define JSON_MAX_TOKENS (128)
# define JSON_BUF_KEY_SIZE (128)

// JSMN error bindings err -> string literals
# define JSON_ERROR(err_str, err_code)                                         \
    switch (err_code)                                                          \
    {                                                                          \
        case JSMN_ERROR_INVAL:                                                 \
            err_str = "Encountered a bad token, JSON string is corrupted";     \
            break;                                                             \
        case JSMN_ERROR_NOMEM:                                                 \
            err_str = "Not enough tokens allocated, JSON string is too large"; \
            break;                                                             \
        case JSMN_ERROR_PART:                                                  \
            err_str = "JSON string is too short, expecting more JSON data";    \
            break;                                                             \
        default:                                                               \
            err_str = "Unexpected JSON parsing error";                         \
    }

/**
 * @brief Module entry point to be referenced by nginx
 */
ngx_module_t ngx_http_couchlookup_module;

/**
 * @brief Module configuration
 */
typedef struct {
    ngx_http_complex_value_t *complex_couch_key;
    lcb_t couch_instance;
    ngx_http_hashtb_table_s *aqvars;
} ngx_http_couchlookup_conf_s;

/**
 * @brief Variable stored in mcf->aqvars
 */
typedef struct {
    ngx_str_t *name;
    ngx_int_t index;
} ngx_http_aqvar_s;

#endif // !NGX_HTTP_COUCHLOOKUP_MODULE_H
