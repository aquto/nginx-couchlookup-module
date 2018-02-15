#include <ngx_core.h>
#include "ngx_http_hashtb.h"

static int hash(unsigned size, ngx_str_t *key)
{
    // djb2 hash function adapted from: http://www.cse.yorku.ca/~oz/hash.html
    unsigned long hash = 5381;
    unsigned p = 0;

    while (p < key->len)
        hash = ((hash << 5) + hash) + key->data[p++]; /* hash * 33 + data[p] */

    return hash % size;
}

static int get_pos_for_elt(ngx_http_hashtb_table_s *tb, ngx_str_t *key)
{
    int h = hash(tb->size, key);
    int end = h + tb->size;
    int i;
    for (i = h; i < end; ++i)
    {
        int pos = i % tb->size;
        if (tb->elts[pos] == NULL || ngx_strcmp(tb->elts[pos]->key->data, key->data) == 0)
            return pos;
    }

    return -1;
}

ngx_http_hashtb_table_s *ngx_http_hashtb_init(ngx_pool_t *pool, unsigned size)
{
    if (size > UINT_MAX / 2 || size == 0)
        return NULL;

    ngx_http_hashtb_table_s *tb = ngx_palloc(pool, sizeof (ngx_http_hashtb_table_s));
    tb->pool = pool;
    tb->size = size;
    size_t sz = sizeof (ngx_http_hashtb_elt_s) * size;
    tb->elts = ngx_palloc(tb->pool, sz);
    memset(tb->elts, 0, sz);

    return tb;
}

ngx_http_hashtb_status_e ngx_http_hashtb_add(ngx_http_hashtb_table_s *tb, ngx_str_t *key, void *data)
{
    int h = get_pos_for_elt(tb, key);
    if (h != -1 && tb->elts[h] == NULL)
    {
        ngx_http_hashtb_elt_s *elt = ngx_palloc(tb->pool, sizeof (ngx_http_hashtb_elt_s));
        elt->key = key;
        elt->data = data;
        tb->elts[h] = elt;
        return HTB_ADD_SUCCESS;
    }
    return HTB_ADD_FAILURE;
}

void *ngx_http_hashtb_get(ngx_http_hashtb_table_s *tb, ngx_str_t *key)
{
    int h = get_pos_for_elt(tb, key);
    if (h != -1 && tb->elts[h] != NULL)
        return tb->elts[h]->data;
    else
        return NULL;
}

/**
 * For reference, if we end up needing to remove elements, it can be done as such:
 *
 * void ngx_http_hashtb_remove(ngx_http_hashtb_table_s *tb, ngx_str_t *key)
 * {
 *     int h = get_pos_for_elt(tb, key);
 *     if (h != -1 && tb->elts[h] != NULL)
 *     {
 *         ngx_pfree(tb->pool, tb->elts[h]);
 *         tb->elts[h] = NULL;
 *     }
 * }
 */

/**
 * For reference, if we end up needing to free the hashtable, it can be done as such:
 *
 * void ngx_http_hashtb_free(ngx_http_hashtb_table_s *tb)
 * {
 *     unsigned i;
 *     for (i = 0; i < tb->size; ++i)
 *         if (tb->elts[i] != NULL)
 *             ngx_pfree(tb->pool, tb->elts[i]);
 *
 *     ngx_pfree(tb->pool, tb->elts);
 *     ngx_pfree(tb->pool, tb);
 * }
 */
