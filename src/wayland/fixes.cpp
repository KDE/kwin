/*
    SPDX-FileCopyrightText: 2024 Joaquim Monteiro <joaquim.monteiro@protonmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "fixes.h"

#include "config-kwin.h"
#if HAVE_WL_FIXES

#include "display.h"
#include "qwayland-server-wayland.h"

namespace KWin
{
static constexpr int s_version = 1;

class FixesInterfacePrivate : public QtWaylandServer::wl_fixes
{
public:
    FixesInterfacePrivate(Display *display);

protected:
    void fixes_destroy(Resource *resource) override;
    void fixes_destroy_registry(Resource *resource, struct ::wl_resource *registry) override;
};

FixesInterfacePrivate::FixesInterfacePrivate(Display *display)
    : QtWaylandServer::wl_fixes(*display, s_version)
{
}

void FixesInterfacePrivate::fixes_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void FixesInterfacePrivate::fixes_destroy_registry(Resource *resource, struct ::wl_resource *registry)
{
    wl_resource_destroy(registry);
}

FixesInterface::FixesInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d{std::make_unique<FixesInterfacePrivate>(display)}
{
}

FixesInterface::~FixesInterface()
{
}

} // namespace KWin

#endif // HAVE_WL_FIXES
