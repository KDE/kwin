/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "screens_virtual.h"
#include "virtual_backend.h"
#include "virtual_output.h"

namespace KWin
{

VirtualScreens::VirtualScreens(VirtualBackend *backend, QObject *parent)
    : OutputScreens(backend, parent)
    , m_backend(backend)
{
}

VirtualScreens::~VirtualScreens() = default;

void VirtualScreens::init()
{
    updateCount();
    KWin::Screens::init();

    connect(m_backend, &VirtualBackend::virtualOutputsSet, this,
        [this] (bool countChanged) {
            if (countChanged) {
                setCount(m_backend->outputs().size());
            } else {
                emit changed();
            }
        }
    );

    emit changed();
}

}
