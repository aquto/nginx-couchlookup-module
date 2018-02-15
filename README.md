nginx-couchlookup-module
========================

Nginx module to lookup Couchbase documents and register their top-level JSON keys as configuration variables.

Build steps
-----------

TODO

Example usage
-------------

### Credentials

Located in `/etc/couch_creds.conf`

```
localhost:testbucket:username:password
```

### Documents

Prefix: `doc_`

**Document 1:** `doc_123`

```
{
    "type": "redirect",
    "url": "https://www.aquto.com/"
}
```

**Document 2:** `doc_456`

```
{
    "type": "proxy",
    "url": "https://www.wikipedia.org/"
}
```

### Nginx config

```
server {
    listen 80;

    location ~ /lookup/(.*)$ {
        resolver 8.8.8.8; # For proxy_pass when using a variable

        couchlookup_creds /etc/couch_creds.conf;
        couchlookup_read_doc "doc_$1" "type,url"; # -> $cl_type, $cl_url

        # Logic branching
        if ($cl_type = "redirect") {
            return 307 $cl_url;
        }
        if ($cl_type = "proxy") {
            proxy_pass $cl_url;
            break; # Needed to avoid 404
        }
        return 404;
    }
}
```

### Using it

**Document 1:**

```
$ curl -v localhost/123
*   Trying 127.0.0.1...
* Connected to localhost (127.0.0.1) port 80 (#0)
> GET /123 HTTP/1.1
> Host: localhost
> User-Agent: curl/7.43.0
> Accept: */*
>
< HTTP/1.1 307 Temporary Redirect
< Server: nginx/1.12.1
< Date: Tue, 15 Feb 2018 15:28:13 GMT
< Content-Type: text/html
< Content-Length: 187
< Connection: keep-alive
< Location: https://www.aquto.com/
<
...
```

**Document 2:**

```
$ curl -v localhost/456
*   Trying 127.0.0.1...
* Connected to localhost (127.0.0.1) port 80 (#0)
> GET /123 HTTP/1.1
> Host: localhost
> User-Agent: curl/7.43.0
> Accept: */*
>
< HTTP/1.1 200 OK
< Server: nginx/1.12.1
< Date: Thu, 15 Feb 2018 22:35:10 GMT
< Content-Type: text/html
< Content-Length: 187
< Connection: keep-alive
<
<!DOCTYPE html>
<html lang="mul" class="no-js">
<head>
<meta charset="utf-8">
<title>Wikipedia</title>
...
```

Third party libraries
---------------------

* [jsmn](https://github.com/zserge/jsmn): a copy of the source is shipped with this project in the `lib` directory
* [libcouchbase](https://github.com/couchbase/libcouchbase): referenced at compilation using the `-lcouchbase` flag
