#include "http_serv.h"
#include "config.h"

#include <MESA/MESA_prof_load.h>
#include <assert.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <pthread.h>
#include <stdio.h>

struct http_serv_main * http_server_main = NULL;

struct http_serv_main
{
    struct event_base * ev_base;
    struct evhttp * ev_http;
    pthread_t ev_dispatch_tid;
};

static void http_serv_probe_cb(struct evhttp_request * req, void * arg)
{
    struct evbuffer * evb = evbuffer_new();
    if (!evb)
    {
        dzlog_error("failed to create response buffer");
        return;
    }

    evbuffer_add_printf(evb, "Probe successfully, dp_trace_telemetry is running.");
    evhttp_send_reply(req, HTTP_OK, "OK", evb);
    evbuffer_free(evb);
}

/* event_base dispatch thread */
static void * http_serv_dispatch(void * arg)
{
    struct event_base * base = (struct event_base *)arg;
    event_base_dispatch(base);
    return NULL;
}

int http_serv_deinit()
{
    assert(http_server_main != NULL);

    /* stop event_base dispatch thread */
    event_base_loopexit(http_server_main->ev_base, NULL);
    pthread_join(http_server_main->ev_dispatch_tid, NULL);

    /* release all ctx */
    evhttp_free(http_server_main->ev_http);
    event_base_free(http_server_main->ev_base);

    /* free http_serv_main */
    free(http_server_main);
    return 0;
}

int http_serv_init()
{
    int ret = 0;
    http_server_main = calloc(1, sizeof(struct http_serv_main));
    DP_TRACE_VERIFY(http_server_main, "http_server_main calloc failed");

    struct event_config * cfg = event_config_new();
    struct event_base * base = event_base_new_with_config(cfg);
    DP_TRACE_VERIFY(base, "Couldn't create an event_base.");

    event_config_free(cfg);
    cfg = NULL;

    /* Create a new evhttp object to socket requests. */
    struct evhttp * http = evhttp_new(base);
    DP_TRACE_VERIFY(http, "couldn't create evhttp. Exiting.");

    const struct config * conf = global_config_get();

    evhttp_set_cb(http, conf->keep_alive_path, http_serv_probe_cb, (void *)http_server_main);

    struct evhttp_bound_socket * socket =
        evhttp_bind_socket_with_handle(http, conf->str_listen_addr, conf->listen_port);
    DP_TRACE_VERIFY(socket, "couldn't bind to %s:%d.", conf->str_listen_addr, conf->listen_port);

    /* create a thread to do the event_base dispatch */
    pthread_t tid;
    ret = pthread_create(&tid, NULL, http_serv_dispatch, base);
    DP_TRACE_VERIFY(ret == 0, "failed to create thread for monit_loop.return value:%d", ret);

    /* save the event_base, ev_http handles to http_serv_main */
    http_server_main->ev_base = base;
    http_server_main->ev_http = http;
    http_server_main->ev_dispatch_tid = tid;

    dzlog_info("HTTP Server is listening on %s:%u", conf->str_listen_addr, conf->listen_port);
    dzlog_info("keep alive path:%s", conf->keep_alive_path);
    return 0;
}
