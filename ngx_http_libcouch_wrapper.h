#ifndef NGX_HTTP_LIBCOUCH_WRAPPER_H
# define NGX_HTTP_LIBCOUCH_WRAPPER_H

# include <ngx_core.h>
# include <libcouchbase/couchbase.h>

/**
 * @brief Couchbase connection string with host and bucket
 * @param %s [1] host
 * @param %s [2] bucket
 */
# define LCW_COUCH_CONN_STR ("couchbase://%s/%s")

/**
 * @brief Couchbase credentials, host and bucket
 */
typedef struct {
    char *host;
    char *bucket;
    char *username;
    char *password;
} lcw_creds_s;

/**
 * @brief Couchbase GET result
 * @details Contains libcouchbase status code for error handling and \
 *  nginx allocation pool for allocation of its `data` pointer.
 */
typedef struct {
    u_char *data; // document contents
    size_t len; // document size
    lcb_error_t status; // couchbase operation status
    ngx_pool_t *pool; // nginx allocation pool
} lcw_get_result_s;

/**
 * @brief Creates new couchbase instance
 */
lcb_t lcw_init(ngx_pool_t *pool, const lcw_creds_s *creds);

/**
 * @brief GET call to retrieve a couchbase document
 */
lcw_get_result_s *lcw_get(ngx_pool_t *pool, lcb_t instance, ngx_str_t *couch_key);

/**
 * @brief Deallocates a couchbase GET result
 */
void lcw_get_result_destroy(lcw_get_result_s *get_res);

/**
 * @brief Deallocates a couchbase instance
 *
 * void lcw_destroy(lcb_t instance);
 */

#endif // !NGX_HTTP_LIBCOUCH_WRAPPER_H
