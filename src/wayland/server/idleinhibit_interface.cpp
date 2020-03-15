/*
    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "idleinhibit_interface_p.h"

namespace KWayland
{
namespace Server
{

IdleInhibitManagerInterface::Private::Private(IdleInhibitManagerInterface *q, Display *d, const wl_interface *interface, quint32 version, IdleInhibitManagerInterfaceVersion interfaceVersion)
    : Global::Private(d, interface, version)
    , interfaceVersion(interfaceVersion)
    , q(q)
{
}

IdleInhibitManagerInterface::IdleInhibitManagerInterface(Private *d, QObject *parent)
    : Global(d, parent)
{
}

IdleInhibitManagerInterface::~IdleInhibitManagerInterface() = default;

IdleInhibitManagerInterfaceVersion IdleInhibitManagerInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion;
}

IdleInhibitManagerInterface::Private *IdleInhibitManagerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
