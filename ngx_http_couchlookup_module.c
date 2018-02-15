#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_http.h>
#include "ngx_http_couchlookup_module.h"
#include "ngx_http_libcouch_wrapper.h"
#include "lib/jsmn.h"

/**
 * @brief Variable handler
 * @param r Pointer to the request structure, see http_request.h
 * @param v Variable value, unused here
 * @param data Pointer to module config in this case
 */
static ngx_int_t ngx_http_couchlookup_variable_handler(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    // Handler called every time the variable is referenced in the config
    if (v->data != NULL) // No need to fetch its value if it's already set
        return NGX_OK;

    // Getting module configuration through the `data` argument, cannot use
    // ngx_http_get_module_loc_conf in this instance (only for request handler).
    ngx_http_couchlookup_conf_s *mcf = (ngx_http_couchlookup_conf_s *)data;

    ngx_str_t couch_key;
    if (ngx_http_complex_value(r, mcf->complex_couch_key, &couch_key) != NGX_OK)
        return NGX_ERROR;

    lcw_get_result_s *couch_doc = lcw_get(r->pool, mcf->couch_instance, &couch_key);
    if (couch_doc == NULL || couch_doc->status != LCB_SUCCESS)
    {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
            "Could not read couch document: %s", lcb_strerror(NULL, couch_doc->status));
        goto failed;
    }

    // Parsing JSON in couch document
    jsmn_parser jparser;
    jsmntok_t tokens[JSON_MAX_TOKENS];
    jsmn_init(&jparser);
    int tok_res = jsmn_parse(&jparser, (char *)couch_doc->data, couch_doc->len, tokens, JSON_MAX_TOKENS);
    if (tok_res < 0)
    {
        const char *err_str;
        JSON_ERROR(err_str, tok_res);
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
            "Could not parse JSON from couch document: %s", err_str);
        goto failed;
    }

    // Make sure the top-level element is an object
    if (tok_res < 1 || tokens[0].type != JSMN_OBJECT)
    {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
            "Top-level JSON element in couch document needs to be an object");
        goto failed;
    }

    int ti; // token index
    int pos = 0;
    int last_tok_key = tok_res - 1;
    for (ti = 1; ti < last_tok_key; ++ti)
    {
        jsmntok_t tok_key = tokens[ti];
        jsmntok_t tok_val = tokens[ti + 1];
        if (tok_key.start < pos || tok_key.type != JSMN_STRING) // only handling top-level elements
            continue;

        ti += tok_key.size; // skipping children, no need to loop through them
        pos = tok_val.end; // avoiding arrays of objects, etc.

        char buf_key[JSON_BUF_KEY_SIZE];
        size_t key_len = tok_key.end - tok_key.start;
        if (key_len > sizeof (buf_key))
        {
            ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                "Variable not set because JSON top-level key length is exceeding " \
                "size limit of %d characters: %*s",
                JSON_BUF_KEY_SIZE, key_len, couch_doc->data + tok_key.start);
            continue;
        }
        ngx_memcpy(buf_key, couch_doc->data + tok_key.start, key_len);
        buf_key[key_len] = '\0';

        size_t var_len = snprintf(NULL, 0, VAR_NAME_TPL, buf_key);
        char buf_varname[var_len];
        if (sprintf((char *)buf_varname, VAR_NAME_TPL, buf_key) < 0)
            continue;

        ngx_str_t var_name = { .data = (u_char *)buf_varname, .len = var_len };
        ngx_http_aqvar_s *res = ngx_http_hashtb_get(mcf->aqvars, &var_name);
        if (res != NULL)
        {
            ngx_http_variable_value_t *var = &r->variables[res->index];

            var->len = tok_val.end - tok_val.start;
            var->data = ngx_palloc(r->pool, sizeof (u_char) * (var->len + 1));
            ngx_memcpy(var->data, couch_doc->data + tok_val.start, var->len);
            var->data[var->len] = '\0';
        } // no failure case, var can be absent from JSON
    }

    unsigned vi; // `variable index`, needed in for loop after failed label
failed:
    if (couch_doc != NULL)
        lcw_get_result_destroy(couch_doc);

    // Assigning an empty value to all the remaining untouched variables
    for (vi = 0; vi < mcf->aqvars->size; ++vi)
    {
        if (mcf->aqvars->elts[vi] == NULL)
            continue;

        ngx_http_aqvar_s *var = mcf->aqvars->elts[vi]->data;
        ngx_http_variable_value_t *var_value = &r->variables[var->index];
        if (var_value->data == NULL)
        {
            // This macro needs to be surrounded by curly brackets to ensure
            // expected behaviour (contains multiple lines).
            ngx_str_set(var_value, "");
        }
    }

    return NGX_OK;
}

/**
 * @brief Configuration setup for credentials file
 * @param cf Module configuration structure pointer
 * @param cmd Module directives structure pointer
 * @param conf Module configuration structure pointer
 * @returns string Status of the configuration setup
 */
static char *ngx_http_couchlookup_creds(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    // Module config
    ngx_http_couchlookup_conf_s *mcf = conf;

    ngx_str_t *value = cf->args->elts;
    ngx_str_t *creds_file = &value[1]; // value[0] is the command name

    char *rc = NGX_CONF_ERROR;
    char *buf = NULL;

    ngx_fd_t fd = ngx_open_file(creds_file->data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE)
    {
        ngx_log_stderr(0, "Could not open file: %*s",
            creds_file->len, creds_file->data);
        goto failure;
    }

    ngx_file_info_t fi;
    if (ngx_fd_info(fd, &fi) == NGX_FILE_ERROR)
    {
        ngx_log_stderr(0, "Could not access file info for: %*s",
            creds_file->len, creds_file->data);
        goto failure;
    }

    size_t size = ngx_file_size(&fi);
    buf = ngx_alloc(size, cf->log);
    if (buf == NULL)
        goto failure;

    ssize_t n = ngx_read_fd(fd, buf, size);
    if (n == -1)
    {
        ngx_log_stderr(0, "Could not read from file descriptor: %*s",
            creds_file->len, creds_file->data);
        goto failure;
    }
    else if ((size_t) n != size)
    {
        ngx_log_stderr(0, "File descriptor has read only %z of %O from %*s",
            n, size, creds_file->len, creds_file->data);
        goto failure;
    }

    lcw_creds_s creds = { 0 };
    SET_FIRST_CREDS_TOK(creds.host, buf);
    SET_NEXT_CREDS_TOK(creds.bucket);
    SET_NEXT_CREDS_TOK(creds.username);
    SET_NEXT_CREDS_TOK(creds.password);

    mcf->couch_instance = lcw_init(cf->pool, &creds);
    if (mcf->couch_instance == NULL)
        goto failure;

    rc = NGX_CONF_OK;

failure:
    if (fd != NGX_INVALID_FILE && ngx_close_file(fd) == NGX_FILE_ERROR)
        ngx_log_stderr(0, "Could not close file descriptor for file: %*s",
            creds_file->len, creds_file->data);

    if (buf != NULL)
        ngx_free(buf);

    return rc;
}

/**
 * @brief Configuration setup for couch key and variables to declare
 * @param cf Module configuration structure pointer
 * @param cmd Module directives structure pointer
 * @param conf Module configuration structure pointer
 * @returns string Status of the configuration setup
 */
static char *ngx_http_couchlookup_read_doc(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    // Module config
    ngx_http_couchlookup_conf_s *mcf = conf;
    if (mcf->couch_instance == NULL)
    {
        ngx_log_stderr(0, "Couchbase instance not found. Hint: couchlookup_read_doc needs " \
            "to be placed after couchlookup_creds.");
        return NGX_CONF_ERROR;
    }

    mcf->complex_couch_key = ngx_palloc(cf->pool, sizeof (ngx_http_complex_value_t));
    if (mcf->complex_couch_key == NULL)
        return NGX_CONF_ERROR;

    ngx_str_t *value = cf->args->elts;

    // Handling first parameter: couchbase key
    if (ngx_http_script_variables_count(&value[1]) != 1)
    {
        ngx_log_stderr(0, "couchlookup_read_doc will only interpret one variable");
        return NGX_CONF_ERROR;
    }

    ngx_http_compile_complex_value_t ccv;
    ngx_memzero(&ccv, sizeof (ngx_http_compile_complex_value_t));
    ccv.cf = cf;
    ccv.value = &value[1]; // value[0] is the command name
    ccv.complex_value = mcf->complex_couch_key;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK)
        return NGX_CONF_ERROR;

    // Handling second parameter: variable names
    char *name_tok = strtok((char *)value[2].data, ",");
    do
    {
        ngx_str_t *var_name = ngx_palloc(cf->pool, sizeof (ngx_str_t));
        if (var_name == NULL)
            return NGX_CONF_ERROR;
        var_name->len = snprintf(NULL, 0, VAR_NAME_TPL, name_tok);
        var_name->data = ngx_palloc(cf->pool, sizeof (u_char) * (var_name->len + 1));
        if (sprintf((char *)var_name->data, VAR_NAME_TPL, name_tok) < 0)
            return NGX_CONF_ERROR;

        ngx_http_variable_t *var = ngx_http_add_variable(cf, var_name, NGX_HTTP_VAR_CHANGEABLE);
        if (var == NULL)
            return NGX_CONF_ERROR;
        var->get_handler = ngx_http_couchlookup_variable_handler;
        var->data = (uintptr_t)mcf;

        ngx_http_aqvar_s *aqvar = ngx_palloc(cf->pool, sizeof (ngx_http_aqvar_s));
        if (aqvar == NULL)
            return NGX_CONF_ERROR;
        aqvar->name = var_name;
        // Calling ngx_http_get_variable_index registers the variable index in request->variables
        aqvar->index = ngx_http_get_variable_index(cf, var_name);
        if (ngx_http_hashtb_add(mcf->aqvars, aqvar->name, aqvar) == HTB_ADD_FAILURE)
            return NGX_CONF_ERROR;

        name_tok = strtok(NULL, ",");
    }
    while (name_tok != NULL);

    return NGX_CONF_OK;
}

/**
 * @brief Nginx configuration mappings
 * @details Flags info: http://www.nginxguts.com/2011/09/configuration-directives/
 */
static ngx_command_t ngx_http_couchlookup_commands[] = {
    { ngx_string("couchlookup_creds"),      // directive name
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1, // context and arguments
      ngx_http_couchlookup_creds,           // configuration setup function
      NGX_HTTP_LOC_CONF_OFFSET,         // offset of the field in the conf data struct
      0,                                // offset when storing the module conf on struct
      NULL },

    { ngx_string("couchlookup_read_doc"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_http_couchlookup_read_doc,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command // command termination
};

/**
 * @brief Allocation/init of the configuration in memory
 * @returns Pointer to allocated module configuration
 */
static void *ngx_http_couchlookup_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_couchlookup_conf_s *mcf;
    if ((mcf = ngx_pcalloc(cf->pool, sizeof (ngx_http_couchlookup_conf_s))) == NULL)
        return NULL;

    mcf->complex_couch_key = NULL;
    mcf->couch_instance = NULL;
    if ((mcf->aqvars = ngx_http_hashtb_init(cf->pool, VAR_HTB_SIZE)) == NULL)
        return NULL;

    return mcf;
}

/**
 * @brief Module context and configuration bindings
 */
static ngx_http_module_t ngx_http_couchlookup_module_ctx = {
    NULL,                                 // preconfiguration
    NULL,                                 // postconfiguration

    NULL,                                 // create main configuration
    NULL,                                 // init main configuration

    NULL,                                 // create server configuration
    NULL,                                 // merge server configuration

    ngx_http_couchlookup_create_loc_conf, // create location configuration
    NULL                                  // merge location configuration
};

/**
 * @brief Module definition
 */
ngx_module_t ngx_http_couchlookup_module = {
    NGX_MODULE_V1,
    &ngx_http_couchlookup_module_ctx, // module context
    ngx_http_couchlookup_commands,    // module directives
    NGX_HTTP_MODULE,                  // module type
    NULL,                             // init master
    NULL,                             // init module
    NULL,                             // init process
    NULL,                             // init thread
    NULL,                             // exit thread
    NULL,                             // exit process
    NULL,                             // exit master
    NGX_MODULE_V1_PADDING
};
