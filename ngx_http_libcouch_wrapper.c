#include <libcouchbase/couchbase.h>
#include "ngx_http_libcouch_wrapper.h"

static void lcw_get_handler(lcb_t instance, int cbtype, const lcb_RESPBASE *rb)
{
    lcw_get_result_s *get_res = rb->cookie;
    get_res->status = rb->rc;
    if (get_res->status == LCB_SUCCESS)
    {
        const lcb_RESPGET *resp = (const lcb_RESPGET*)rb;
        get_res->len = resp->nvalue;
        get_res->data = ngx_pcalloc(get_res->pool, get_res->len);
        ngx_memcpy(get_res->data, resp->value, resp->nvalue);
    }
}

lcb_t lcw_init(ngx_pool_t *pool, const lcw_creds_s *creds)
{
    lcb_t instance = NULL;

    size_t connstr_len = 1 + snprintf(NULL, 0, LCW_COUCH_CONN_STR, creds->host, creds->bucket);
    char *connstr = ngx_palloc(pool, sizeof (char) * connstr_len);
    if (connstr == NULL ||
        sprintf(connstr, LCW_COUCH_CONN_STR, creds->host, creds->bucket) < 0)
        goto failure;

    struct lcb_create_st cropts = {
        .version = 3,
        .v.v3 = {
            .connstr = connstr,
            .username = creds->username,
            .passwd = creds->password
        }
    };

    lcb_error_t err = lcb_create(&instance, &cropts);
    if (err != LCB_SUCCESS)
    {
        ngx_log_stderr(0, "Could not create couchbase instance: %s", lcb_strerror(NULL, err));
        lcb_destroy(instance);
        instance = NULL;
        goto failure;
    }

    lcb_connect(instance);
    lcb_wait(instance);
    if ((err = lcb_get_bootstrap_status(instance)) != LCB_SUCCESS)
    {
        ngx_log_stderr(0, "Could not bootstrap couchbase instance: %s", lcb_strerror(NULL, err));
        lcb_destroy(instance);
        instance = NULL;
        goto failure;
    }

    lcb_install_callback3(instance, LCB_CALLBACK_GET, lcw_get_handler);

failure:
    if (connstr != NULL)
        ngx_pfree(pool, connstr);

    return instance;
}

lcw_get_result_s *lcw_get(ngx_pool_t *pool, lcb_t instance, ngx_str_t *couch_key)
{
    lcw_get_result_s *get_res = ngx_palloc(pool, sizeof (lcw_get_result_s));
    if (get_res == NULL)
        return NULL;

    get_res->data = NULL;
    get_res->pool = pool;

    lcb_CMDGET gcmd;
    ngx_memzero(&gcmd, sizeof (gcmd));

    LCB_CMD_SET_KEY(&gcmd, couch_key->data, couch_key->len);
    lcb_get3(instance, get_res, &gcmd);
    lcb_wait(instance);

    return get_res;
}

void lcw_get_result_destroy(lcw_get_result_s *get_res)
{
    ngx_pfree(get_res->pool, get_res->data);
    ngx_pfree(get_res->pool, get_res);
}

/**
 * For reference, this is how the couchbase instance can be destroyed:
 *
 * void lcw_destroy(lcb_t instance)
 * {
 *     lcb_destroy(instance);
 * }
 */
