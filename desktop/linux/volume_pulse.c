#include "volume_pulse.h"
#include "rc_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>

#define VOLUME_STEP_PERCENT 5

static pa_mainloop     *g_mainloop = NULL;
static pa_mainloop_api *g_api = NULL;
static pa_context      *g_context = NULL;

/* Callback: PulseAudio context state */
static void context_state_cb(pa_context *ctx, void *userdata)
{
    (void)userdata;
    (void)ctx;
}

int rc_volume_init(void)
{
    g_mainloop = pa_mainloop_new();
    if (g_mainloop == NULL) {
        fprintf(stderr, "[volume] Failed to create PulseAudio mainloop\n");
        return -1;
    }

    g_api = pa_mainloop_get_api(g_mainloop);
    assert(g_api != NULL);

    g_context = pa_context_new(g_api, "RemoteControl");
    if (g_context == NULL) {
        fprintf(stderr, "[volume] Failed to create PulseAudio context\n");
        pa_mainloop_free(g_mainloop);
        return -1;
    }

    pa_context_set_state_callback(g_context, context_state_cb, NULL);

    if (pa_context_connect(g_context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        fprintf(stderr, "[volume] Failed to connect to PulseAudio: %s\n",
                pa_strerror(pa_context_errno(g_context)));
        pa_context_unref(g_context);
        pa_mainloop_free(g_mainloop);
        return -1;
    }

    /* Wait for connection to be ready */
    int retval = 0;
    for (int i = 0; i < 100; i++) {
        pa_mainloop_iterate(g_mainloop, 0, &retval);
        pa_context_state_t state = pa_context_get_state(g_context);
        if (state == PA_CONTEXT_READY) {
            fprintf(stdout, "[volume] PulseAudio connected\n");
            return 0;
        }
        if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
            break;
        }
    }

    fprintf(stderr, "[volume] PulseAudio connection timed out\n");
    pa_context_unref(g_context);
    pa_mainloop_free(g_mainloop);
    g_context = NULL;
    g_mainloop = NULL;
    return -1;
}

void rc_volume_shutdown(void)
{
    if (g_context != NULL) {
        pa_context_disconnect(g_context);
        pa_context_unref(g_context);
        g_context = NULL;
    }
    if (g_mainloop != NULL) {
        pa_mainloop_free(g_mainloop);
        g_mainloop = NULL;
    }
}

/* Callback for sink info */
typedef struct {
    uint8_t action;
    pa_mainloop *loop;
} vol_action_t;

static void sink_info_cb(pa_context *ctx, const pa_sink_info *info,
                         int eol, void *userdata)
{
    if (eol > 0 || info == NULL) return;

    vol_action_t *act = (vol_action_t *)userdata;
    assert(act != NULL);

    pa_cvolume volume = info->volume;
    pa_volume_t step = (pa_volume_t)(PA_VOLUME_NORM * VOLUME_STEP_PERCENT / 100);

    switch (act->action) {
    case RC_VOL_UP:
        pa_cvolume_inc(&volume, step);
        pa_context_set_sink_volume_by_index(ctx, info->index, &volume, NULL, NULL);
        break;
    case RC_VOL_DOWN:
        pa_cvolume_dec(&volume, step);
        pa_context_set_sink_volume_by_index(ctx, info->index, &volume, NULL, NULL);
        break;
    case RC_VOL_MUTE:
        pa_context_set_sink_mute_by_index(ctx, info->index, !info->mute, NULL, NULL);
        break;
    }
}

void rc_volume_handle(uint8_t action)
{
    if (g_context == NULL || g_mainloop == NULL) return;
    if (action > RC_VOL_MUTE) return;

    vol_action_t act = { .action = action, .loop = g_mainloop };

    /* Get default sink and modify volume */
    pa_operation *op = pa_context_get_sink_info_by_name(
        g_context, "@DEFAULT_SINK@", sink_info_cb, &act);

    if (op != NULL) {
        /* Process the operation */
        int retval = 0;
        for (int i = 0; i < 50; i++) {
            pa_mainloop_iterate(g_mainloop, 0, &retval);
            if (pa_operation_get_state(op) != PA_OPERATION_RUNNING) break;
        }
        pa_operation_unref(op);

        /* Process the set volume command */
        pa_mainloop_iterate(g_mainloop, 0, &retval);
    }
}
