#ifndef NGX_HTTP_HASHTB_H
# define NGX_HTTP_HASHTB_H

/**
 * @brief Hashtable element
 */
typedef struct
{
    ngx_str_t *key;
    void *data;
} ngx_http_hashtb_elt_s;

/**
 * @brief Hashtable structure
 */
typedef struct
{
    ngx_pool_t *pool; // module conf memory pool
    ngx_http_hashtb_elt_s **elts;
    unsigned size;
} ngx_http_hashtb_table_s;

/**
 * @brief Enum of operation statuses
 */
typedef enum {
    HTB_ADD_SUCCESS,
    HTB_ADD_FAILURE
} ngx_http_hashtb_status_e;

/**
 * @brief Allocates new hashtable
 */
ngx_http_hashtb_table_s *ngx_http_hashtb_init(ngx_pool_t *pool, unsigned size);

/**
 * @brief Adds element in hashtable (fails silently if element already present)
 */
ngx_http_hashtb_status_e ngx_http_hashtb_add(ngx_http_hashtb_table_s *tb, ngx_str_t *key, void *data);

/**
 * @brief Get element from hashtable
 * @returns Pointer to element or NULL if not found
 */
void *ngx_http_hashtb_get(ngx_http_hashtb_table_s *tb, ngx_str_t *key);

/**
 * @brief Removes element from hashtable (fails silently)
 *
 * void ngx_http_hashtb_remove(ngx_http_hashtb_table_s *tb, ngx_str_t *key);
 */

/**
 * @brief Deallocates hashtable (leaves elements untouched)
 *
 * void ngx_http_hashtb_free(ngx_http_hashtb_table_s *tb);
 */

#endif // !NGX_HTTP_HASHTB_H
