/*
    SPDX-FileCopyrightText: 2017 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgforeign_interface.h"
#include "xdgforeign_v2_interface_p.h"
#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "surface_interface_p.h"

#include "wayland-xdg-foreign-unstable-v2-server-protocol.h"

#include <QUuid>
#include <QDebug>

namespace KWayland
{
namespace Server
{

XdgForeignInterface::Private::Private(Display *display, XdgForeignInterface *q)
    : q(q)
{
    exporter = new XdgExporterUnstableV2Interface(display, q);
    importer = new XdgImporterUnstableV2Interface(display, q);

    connect(importer, &XdgImporterUnstableV2Interface::transientChanged,
        q, &XdgForeignInterface::transientChanged);
}

XdgForeignInterface::XdgForeignInterface(Display *display, QObject *parent)
    : QObject(parent),
      d(new Private(display, this))
{
}

XdgForeignInterface::~XdgForeignInterface()
{
    delete d->exporter;
    delete d->importer;
    delete d;
}

void XdgForeignInterface::create()
{
    d->exporter->create();
    d->importer->create();
}

bool XdgForeignInterface::isValid()
{
    return d->exporter->isValid() && d->importer->isValid();
}

SurfaceInterface *XdgForeignInterface::transientFor(SurfaceInterface *surface)
{
    return d->importer->transientFor(surface);
}

}
}

