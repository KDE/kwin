/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_layout_switching.h"
#include "deleted.h"
#include "keyboard_layout.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"
#include "xkb.h"

namespace KWin
{

namespace KeyboardLayoutSwitching
{

Policy::Policy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config)
    : QObject(layout)
    , m_config(config)
    , m_xkb(xkb)
    , m_layout(layout)
{
    connect(m_layout, &KeyboardLayout::layoutsReconfigured, this, &Policy::clearCache);
    connect(m_layout, &KeyboardLayout::layoutChanged, this, &Policy::layoutChanged);
}

Policy::~Policy() = default;

void Policy::setLayout(uint index)
{
    const uint previousLayout = m_xkb->currentLayout();
    m_xkb->switchToLayout(index);
    const uint currentLayout = m_xkb->currentLayout();
    if (previousLayout != currentLayout) {
        Q_EMIT m_layout->layoutChanged(currentLayout);
    }
}

std::unique_ptr<Policy> Policy::create(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config, const QString &policy)
{
    if (policy.toLower() == QStringLiteral("desktop")) {
        return std::make_unique<VirtualDesktopPolicy>(xkb, layout, config);
    }
    if (policy.toLower() == QStringLiteral("window")) {
        return std::make_unique<WindowPolicy>(xkb, layout);
    }
    if (policy.toLower() == QStringLiteral("winclass")) {
        return std::make_unique<ApplicationPolicy>(xkb, layout, config);
    }
    return std::make_unique<GlobalPolicy>(xkb, layout, config);
}

const char Policy::defaultLayoutEntryKeyPrefix[] = "LayoutDefault";
const QString Policy::defaultLayoutEntryKey() const
{
    return QLatin1String(defaultLayoutEntryKeyPrefix) % name() % QLatin1Char('_');
}

void Policy::clearLayouts()
{
    const QStringList layoutEntryList = m_config.keyList().filter(defaultLayoutEntryKeyPrefix);
    for (const auto &layoutEntry : layoutEntryList) {
        m_config.deleteEntry(layoutEntry);
    }
}

const QString GlobalPolicy::defaultLayoutEntryKey() const
{
    return QLatin1String(defaultLayoutEntryKeyPrefix) % name();
}

GlobalPolicy::GlobalPolicy(Xkb *xkb, KeyboardLayout *_layout, const KConfigGroup &config)
    : Policy(xkb, _layout, config)
{
    connect(workspace()->sessionManager(), &SessionManager::prepareSessionSaveRequested, this, [this, xkb](const QString &name) {
        clearLayouts();
        if (const uint layout = xkb->currentLayout()) {
            m_config.writeEntry(defaultLayoutEntryKey(), layout);
        }
    });

    connect(workspace()->sessionManager(), &SessionManager::loadSessionRequested, this, [this, xkb](const QString &name) {
        if (xkb->numberOfLayouts() > 1) {
            setLayout(m_config.readEntry(defaultLayoutEntryKey(), 0));
        }
    });
}

GlobalPolicy::~GlobalPolicy() = default;

VirtualDesktopPolicy::VirtualDesktopPolicy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config)
    : Policy(xkb, layout, config)
{
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged,
            this, &VirtualDesktopPolicy::desktopChanged);

    connect(workspace()->sessionManager(), &SessionManager::prepareSessionSaveRequested, this, [this](const QString &name) {
        clearLayouts();

        for (auto i = m_layouts.constBegin(); i != m_layouts.constEnd(); ++i) {
            if (const uint layout = *i) {
                m_config.writeEntry(defaultLayoutEntryKey() % QString::number(i.key()->x11DesktopNumber()), layout);
            }
        }
    });

    connect(workspace()->sessionManager(), &SessionManager::loadSessionRequested, this, [this, xkb](const QString &name) {
        if (xkb->numberOfLayouts() > 1) {
            const auto &desktops = VirtualDesktopManager::self()->desktops();
            for (KWin::VirtualDesktop *const desktop : desktops) {
                const uint layout = m_config.readEntry(defaultLayoutEntryKey() % QString::number(desktop->x11DesktopNumber()), 0u);
                if (layout) {
                    m_layouts.insert(desktop, layout);
                    connect(desktop, &VirtualDesktop::aboutToBeDestroyed, this, [this, desktop]() {
                        m_layouts.remove(desktop);
                    });
                }
            }
            desktopChanged();
        }
    });
}

VirtualDesktopPolicy::~VirtualDesktopPolicy() = default;

void VirtualDesktopPolicy::clearCache()
{
    m_layouts.clear();
}

namespace
{
template<typename T, typename U>
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

void VirtualDesktopPolicy::layoutChanged(uint index)
{
    auto d = VirtualDesktopManager::self()->currentDesktop();
    if (!d) {
        return;
    }
    auto it = m_layouts.find(d);
    if (it == m_layouts.end()) {
        m_layouts.insert(d, index);
        connect(d, &VirtualDesktop::aboutToBeDestroyed, this, [this, d]() {
            m_layouts.remove(d);
        });
    } else {
        if (it.value() == index) {
            return;
        }
        it.value() = index;
    }
}

WindowPolicy::WindowPolicy(KWin::Xkb *xkb, KWin::KeyboardLayout *layout)
    : Policy(xkb, layout)
{
    connect(workspace(), &Workspace::windowActivated, this, [this](Window *window) {
        if (!window) {
            return;
        }
        // ignore some special types
        if (window->isDesktop() || window->isDock()) {
            return;
        }
        setLayout(getLayout(m_layouts, window));
    });
}

WindowPolicy::~WindowPolicy()
{
}

void WindowPolicy::clearCache()
{
    m_layouts.clear();
}

void WindowPolicy::layoutChanged(uint index)
{
    auto window = workspace()->activeWindow();
    if (!window) {
        return;
    }
    // ignore some special types
    if (window->isDesktop() || window->isDock()) {
        return;
    }

    auto it = m_layouts.find(window);
    if (it == m_layouts.end()) {
        m_layouts.insert(window, index);
        connect(window, &Window::windowClosed, this, [this, window]() {
            m_layouts.remove(window);
        });
    } else {
        if (it.value() == index) {
            return;
        }
        it.value() = index;
    }
}

ApplicationPolicy::ApplicationPolicy(KWin::Xkb *xkb, KWin::KeyboardLayout *layout, const KConfigGroup &config)
    : Policy(xkb, layout, config)
{
    connect(workspace(), &Workspace::windowActivated, this, &ApplicationPolicy::windowActivated);

    connect(workspace()->sessionManager(), &SessionManager::prepareSessionSaveRequested, this, [this](const QString &name) {
        clearLayouts();

        for (auto i = m_layouts.constBegin(); i != m_layouts.constEnd(); ++i) {
            if (const uint layout = *i) {
                const QString desktopFileName = i.key()->desktopFileName();
                if (!desktopFileName.isEmpty()) {
                    m_config.writeEntry(defaultLayoutEntryKey() % desktopFileName, layout);
                }
            }
        }
    });

    connect(workspace()->sessionManager(), &SessionManager::loadSessionRequested, this, [this, xkb](const QString &name) {
        if (xkb->numberOfLayouts() > 1) {
            const QString keyPrefix = defaultLayoutEntryKey();
            const QStringList keyList = m_config.keyList().filter(keyPrefix);
            for (const QString &key : keyList) {
                m_layoutsRestored.insert(
                    QStringView(key).mid(keyPrefix.size()).toLatin1(),
                    m_config.readEntry(key, 0));
            }
        }
        m_layoutsRestored.squeeze();
    });
}

ApplicationPolicy::~ApplicationPolicy()
{
}

void ApplicationPolicy::windowActivated(Window *window)
{
    if (!window) {
        return;
    }
    // ignore some special types
    if (window->isDesktop() || window->isDock()) {
        return;
    }
    auto it = m_layouts.constFind(window);
    if (it != m_layouts.constEnd()) {
        setLayout(it.value());
        return;
    };
    for (it = m_layouts.constBegin(); it != m_layouts.constEnd(); it++) {
        if (Window::belongToSameApplication(window, it.key())) {
            const uint layout = it.value();
            setLayout(layout);
            layoutChanged(layout);
            return;
        }
    }
    setLayout(m_layoutsRestored.take(window->desktopFileName()));
    if (const uint index = m_xkb->currentLayout()) {
        layoutChanged(index);
    }
}

void ApplicationPolicy::clearCache()
{
    m_layouts.clear();
}

void ApplicationPolicy::layoutChanged(uint index)
{
    auto window = workspace()->activeWindow();
    if (!window) {
        return;
    }
    // ignore some special types
    if (window->isDesktop() || window->isDock()) {
        return;
    }

    auto it = m_layouts.find(window);
    if (it == m_layouts.end()) {
        m_layouts.insert(window, index);
        connect(window, &Window::windowClosed, this, [this, window]() {
            m_layouts.remove(window);
        });
    } else {
        if (it.value() == index) {
            return;
        }
        it.value() = index;
    }
    // update all layouts for the application
    for (it = m_layouts.begin(); it != m_layouts.end(); it++) {
        if (Window::belongToSameApplication(it.key(), window)) {
            it.value() = index;
        }
    }
}

}
}
