#include "global.h"

namespace KWaylandServer
{

struct DeferredGlobalDestroyData
{
    wl_global *global;
    wl_event_source *eventSource;
    wl_listener displayDestroyed;
};

static void destroyGlobal(DeferredGlobalDestroyData *payload)
{
    wl_global_destroy(payload->global);
    wl_event_source_remove(payload->eventSource);
    delete payload;
}

static int handleTimerEvent(void *data)
{
    destroyGlobal(static_cast<DeferredGlobalDestroyData *>(data));
    return 0;
}

static void handleDisplayDestroyed(wl_listener *listener, void *)
{
    DeferredGlobalDestroyData *data = wl_container_of(listener, data, displayDestroyed);
    destroyGlobal(data);
}

void wl_global_destroy_deferred(wl_global *global)
{
    wl_global_remove(global);
    wl_global_set_user_data(global, nullptr);

    wl_display *display = wl_global_get_display(global);
    wl_event_loop *eventLoop = wl_display_get_event_loop(display);

    DeferredGlobalDestroyData *data = new DeferredGlobalDestroyData;
    data->global = global;
    data->eventSource = wl_event_loop_add_timer(eventLoop, handleTimerEvent, data);
    wl_event_source_timer_update(data->eventSource, 5000);

    data->displayDestroyed.notify = handleDisplayDestroyed;
    wl_display_add_destroy_listener(display, &data->displayDestroyed);
}

} // namespace KWaylandServer
