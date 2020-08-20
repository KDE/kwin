/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

void Policy::setLayout(quint32 layout)
{
    const quint32 previousLayout = m_xkb->currentLayout();
    m_xkb->switchToLayout(layout);
    if (previousLayout != m_xkb->currentLayout()) {
        m_layout->updateNotifier();
    }
}

quint32 Policy::layout() const
{
    return m_xkb->currentLayout();
}

Policy *Policy::create(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config, const QString &policy)
{
    if (policy.toLower() == QStringLiteral("desktop")) {
        return new VirtualDesktopPolicy(xkb, layout, config);
    }
    if (policy.toLower() == QStringLiteral("window")) {
        return new WindowPolicy(xkb, layout);
    }
    if (policy.toLower() == QStringLiteral("winclass")) {
        return new ApplicationPolicy(xkb, layout, config);
    }
    return new GlobalPolicy(xkb, layout, config);
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
    connect(workspace()->sessionManager(), &SessionManager::prepareSessionSaveRequested, this,
        [this] (const QString &name) {
            Q_UNUSED(name)
            clearLayouts();
            if (layout()) {
                m_config.writeEntry(defaultLayoutEntryKey(), layout());
            }
        }
    );

    connect(workspace()->sessionManager(), &SessionManager::loadSessionRequested, this,
        [this, xkb] (const QString &name) {
            Q_UNUSED(name)
            if (xkb->numberOfLayouts() > 1) {
                setLayout(m_config.readEntry(defaultLayoutEntryKey(), 0));
            }
        }
    );
}

GlobalPolicy::~GlobalPolicy() = default;

VirtualDesktopPolicy::VirtualDesktopPolicy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config)
    : Policy(xkb, layout, config)
{
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged,
            this, 						   &VirtualDesktopPolicy::desktopChanged);

    connect(workspace()->sessionManager(), &SessionManager::prepareSessionSaveRequested, this,
        [this] (const QString &name) {
            Q_UNUSED(name)
            clearLayouts();

            for (auto i = m_layouts.constBegin(); i != m_layouts.constEnd(); ++i) {
                if (const uint layout = *i) {
                    m_config.writeEntry(
                                defaultLayoutEntryKey() %
                                    QLatin1String( QByteArray::number(i.key()->x11DesktopNumber()) ),
                                layout);
                }
            }
        }
    );

    connect(workspace()->sessionManager(), &SessionManager::loadSessionRequested, this,
        [this, xkb] (const QString &name) {
            Q_UNUSED(name)
            if (xkb->numberOfLayouts() > 1) {
                for (KWin::VirtualDesktop* const desktop : VirtualDesktopManager::self()->desktops()) {
                    const uint layout = m_config.readEntry(
                                defaultLayoutEntryKey() %
                                    QLatin1String( QByteArray::number(desktop->x11DesktopNumber()) ),
                                0u);
                    if (layout) {
                        m_layouts.insert(desktop, layout);
                        connect(desktop, &VirtualDesktop::aboutToBeDestroyed, this,
                            [this, desktop] {
                                m_layouts.remove(desktop);
                            }
                        );
                    }
                }
                desktopChanged();
            }
        }
    );
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

ApplicationPolicy::ApplicationPolicy(KWin::Xkb* xkb, KWin::KeyboardLayout* layout, const KConfigGroup &config)
    : Policy(xkb, layout, config)
{
    connect(workspace(), &Workspace::clientActivated, this, &ApplicationPolicy::clientActivated);

    connect(workspace()->sessionManager(), &SessionManager::prepareSessionSaveRequested, this,
        [this] (const QString &name) {
            Q_UNUSED(name)
            clearLayouts();

            for (auto i = m_layouts.constBegin(); i != m_layouts.constEnd(); ++i) {
                if (const uint layout = *i) {
                    const QByteArray desktopFileName = i.key()->desktopFileName();
                    if (!desktopFileName.isEmpty()) {
                        m_config.writeEntry(
                                    defaultLayoutEntryKey() % QLatin1String(desktopFileName),
                                    layout);
                    }
                }
            }
        }
    );

    connect(workspace()->sessionManager(), &SessionManager::loadSessionRequested, this,
        [this, xkb] (const QString &name) {
            Q_UNUSED(name)
            if (xkb->numberOfLayouts() > 1) {
                const QString keyPrefix = defaultLayoutEntryKey();
                const QStringList keyList = m_config.keyList().filter(keyPrefix);
                for (const QString& key : keyList) {
                    m_layoutsRestored.insert(
                                key.midRef(keyPrefix.size()).toLatin1(),
                                m_config.readEntry(key, 0));
                }
            }
            m_layoutsRestored.squeeze();
        }
    );
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
    auto it = m_layouts.constFind(c);
    if(it != m_layouts.constEnd()) {
        setLayout(it.value());
        return;
    };
    for (it = m_layouts.constBegin(); it != m_layouts.constEnd(); it++) {
        if (AbstractClient::belongToSameApplication(c, it.key())) {
            setLayout(it.value());
            layoutChanged();
            return;
        }
    }
    setLayout( m_layoutsRestored.take(c->desktopFileName()) );
    if (layout()) {
        layoutChanged();
    }
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
