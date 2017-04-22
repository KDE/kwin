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
#include "abstract_client.h"
#include "deleted.h"
#include "virtualdesktops.h"
#include "workspace.h"
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
    if (policy.toLower() == QStringLiteral("window")) {
        return new WindowPolicy(xkb, layout);
    }
    if (policy.toLower() == QStringLiteral("winclass")) {
        return new ApplicationPolicy(xkb, layout);
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

namespace {
template <typename T, typename U>
quint32 getLayout(const T &layouts, const U &reference)
{
    auto it = layouts.constFind(reference);
    if (it == layouts.constEnd()) {
        return 0;
    } else {
        return it.value();
    }
}
}

void VirtualDesktopPolicy::desktopChanged()
{
    auto d = VirtualDesktopManager::self()->currentDesktop();
    if (!d) {
        return;
    }
    setLayout(getLayout(m_layouts, d));
}

void VirtualDesktopPolicy::layoutChanged()
{
    auto d = VirtualDesktopManager::self()->currentDesktop();
    if (!d) {
        return;
    }
    auto it = m_layouts.find(d);
    const auto l = layout();
    if (it == m_layouts.end()) {
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

WindowPolicy::WindowPolicy(KWin::Xkb* xkb, KWin::KeyboardLayout* layout)
    : Policy(xkb, layout)
{
    connect(workspace(), &Workspace::clientActivated, this,
        [this] (AbstractClient *c) {
            if (!c) {
                return;
            }
            // ignore some special types
            if (c->isDesktop() || c->isDock()) {
                return;
            }
            setLayout(getLayout(m_layouts, c));
        }
    );
}

WindowPolicy::~WindowPolicy()
{
}

void WindowPolicy::clearCache()
{
    m_layouts.clear();
}

void WindowPolicy::layoutChanged()
{
    auto c = workspace()->activeClient();
    if (!c) {
        return;
    }
    // ignore some special types
    if (c->isDesktop() || c->isDock()) {
        return;
    }

    auto it = m_layouts.find(c);
    const auto l = layout();
    if (it == m_layouts.end()) {
        m_layouts.insert(c, l);
        connect(c, &AbstractClient::windowClosed, this,
            [this, c] {
                m_layouts.remove(c);
            }
        );
    } else {
        if (it.value() == l) {
            return;
        }
        it.value() = l;
    }
}

ApplicationPolicy::ApplicationPolicy(KWin::Xkb* xkb, KWin::KeyboardLayout* layout)
    : Policy(xkb, layout)
{
    connect(workspace(), &Workspace::clientActivated, this, &ApplicationPolicy::clientActivated);
}

ApplicationPolicy::~ApplicationPolicy()
{
}

void ApplicationPolicy::clientActivated(AbstractClient *c)
{
    if (!c) {
        return;
    }
    // ignore some special types
    if (c->isDesktop() || c->isDock()) {
        return;
    }
    quint32 layout = 0;
    for (auto it = m_layouts.constBegin(); it != m_layouts.constEnd(); it++) {
        if (AbstractClient::belongToSameApplication(c, it.key())) {
            layout = it.value();
            break;
        }
    }
    setLayout(layout);
}

void ApplicationPolicy::clearCache()
{
    m_layouts.clear();
}

void ApplicationPolicy::layoutChanged()
{
    auto c = workspace()->activeClient();
    if (!c) {
        return;
    }
    // ignore some special types
    if (c->isDesktop() || c->isDock()) {
        return;
    }

    auto it = m_layouts.find(c);
    const auto l = layout();
    if (it == m_layouts.end()) {
        m_layouts.insert(c, l);
        connect(c, &AbstractClient::windowClosed, this,
            [this, c] {
                m_layouts.remove(c);
            }
        );
    } else {
        if (it.value() == l) {
            return;
        }
        it.value() = l;
    }
    // update all layouts for the application
    for (it = m_layouts.begin(); it != m_layouts.end(); it++) {
        if (!AbstractClient::belongToSameApplication(it.key(), c)) {
            continue;
        }
        it.value() = l;
    }
}

}
}
