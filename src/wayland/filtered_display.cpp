/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "filtered_display.h"
#include "display.h"

#include <wayland-server.h>

#include <QByteArray>

namespace KWin
{
class FilteredDisplayPrivate
{
public:
    FilteredDisplayPrivate(FilteredDisplay *_q);
    FilteredDisplay *q;
    static bool globalFilterCallback(const wl_client *client, const wl_global *global, void *data)
    {
        auto t = static_cast<FilteredDisplayPrivate *>(data);
        auto clientConnection = t->q->getConnection(const_cast<wl_client *>(client));
        auto interface = wl_global_get_interface(global);
        auto name = QByteArray::fromRawData(interface->name, strlen(interface->name));
        return t->q->allowInterface(clientConnection, name);
    };
};

FilteredDisplayPrivate::FilteredDisplayPrivate(FilteredDisplay *_q)
    : q(_q)
{
}

FilteredDisplay::FilteredDisplay(QObject *parent)
    : Display(parent)
    , d(new FilteredDisplayPrivate(this))
{
    connect(this, &Display::runningChanged, [this](bool running) {
        if (!running) {
            return;
        }
        wl_display_set_global_filter(*this, FilteredDisplayPrivate::globalFilterCallback, d.get());
    });
}

FilteredDisplay::~FilteredDisplay()
{
}

}

#include "moc_filtered_display.cpp"
