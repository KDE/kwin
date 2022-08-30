#pragma once

#include <wayland-server-core.h>

namespace KWaylandServer
{

void wl_global_destroy_deferred(wl_global *global);

} // namespace KWaylandServer
