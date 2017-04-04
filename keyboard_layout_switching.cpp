/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

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
#include "keyboard_layout_switching.h"
#include "keyboard_layout.h"
#include "virtualdesktops.h"
#include "xkb.h"

namespace KWin
{

namespace KeyboardLayoutSwitching
{

Policy::Policy(Xkb *xkb, KeyboardLayout *layout)
    : QObject(layout)
    , m_xkb(xkb)
    , m_layout(layout)
{
    connect(m_layout, &KeyboardLayout::layoutsReconfigured, this, &Policy::clearCache);
    connect(m_layout, &KeyboardLayout::layoutChanged, this, &Policy::layoutChanged);
}

Policy::~Policy() = default;

void Policy::setLayout(quint32 layout)
{
    m_xkb->switchToLayout(layout);
}

quint32 Policy::layout() const
{
    return m_xkb->currentLayout();
}

Policy *Policy::create(Xkb *xkb, KeyboardLayout *layout, const QString &policy)
{
    if (policy.toLower() == QStringLiteral("desktop")) {
        return new VirtualDesktopPolicy(xkb, layout);
    }
    return new GlobalPolicy(xkb, layout);
}

GlobalPolicy::GlobalPolicy(Xkb *xkb, KeyboardLayout *layout)
    : Policy(xkb, layout)
{
}

GlobalPolicy::~GlobalPolicy() = default;

VirtualDesktopPolicy::VirtualDesktopPolicy(Xkb *xkb, KeyboardLayout *layout)
    : Policy(xkb, layout)
{
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged, this, &VirtualDesktopPolicy::desktopChanged);
}

VirtualDesktopPolicy::~VirtualDesktopPolicy() = default;

void VirtualDesktopPolicy::clearCache()
{
    m_layouts.clear();
}

void VirtualDesktopPolicy::desktopChanged()
{
    auto d = VirtualDesktopManager::self()->currentDesktop();
    if (!d) {
        return;
    }
    auto it = m_layouts.constFind(d);
    if (it == m_layouts.constEnd()) {
        // new desktop - go to default;
        setLayout(0);
    } else {
        setLayout(it.value());
    }
}

void VirtualDesktopPolicy::layoutChanged()
{
    auto d = VirtualDesktopManager::self()->currentDesktop();
    if (!d) {
        return;
    }
    auto it = m_layouts.find(d);
    const auto l = layout();
    if (it == m_layouts.constEnd()) {
        m_layouts.insert(d, l);
        connect(d, &VirtualDesktop::aboutToBeDestroyed, this,
            [this, d] {
                m_layouts.remove(d);
            }
        );
    } else {
        if (it.value() == l) {
            return;
        }
        it.value() = l;
    }
}

}
}
