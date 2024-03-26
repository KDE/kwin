/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input_event_spy.h"
#include <QList>
#include <QObject>
#include <memory>
#include <optional>

#include <KConfigGroup>
#include <KSharedConfig>
typedef uint32_t xkb_layout_index_t;

class QAction;
class QDBusArgument;

namespace KWin
{
class Xkb;
class KeyboardLayoutDBusInterface;

namespace KeyboardLayoutSwitching
{
class Policy;
}

class KWIN_EXPORT KeyboardLayout : public QObject, public InputEventSpy
{
    Q_OBJECT
public:
    explicit KeyboardLayout(Xkb *xkb, const KSharedConfigPtr &config);

    ~KeyboardLayout() override;

    void init();

    void checkLayoutChange(uint previousLayout);
    void switchToNextLayout();
    void switchToPreviousLayout();
    void switchToLastUsedLayout();
    void resetLayout();

Q_SIGNALS:
    void layoutChanged(uint index);
    void layoutsReconfigured();

private Q_SLOTS:
    void reconfigure();

private:
    void notifyLayoutChange();
    void switchToLayout(xkb_layout_index_t index);
    void loadShortcuts();
    Xkb *m_xkb;
    xkb_layout_index_t m_layout = 0;
    KConfigGroup m_configGroup;
    QList<QAction *> m_layoutShortcuts;
    KeyboardLayoutDBusInterface *m_dbusInterface = nullptr;
    std::unique_ptr<KeyboardLayoutSwitching::Policy> m_policy;
    std::optional<uint> m_lastUsedLayout;
};

class KeyboardLayoutDBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KeyboardLayouts")

public:
    explicit KeyboardLayoutDBusInterface(Xkb *xkb, const KConfigGroup &configGroup, KeyboardLayout *parent);
    ~KeyboardLayoutDBusInterface() override;

    struct LayoutNames
    {
        QString shortName;
        QString displayName;
        QString longName;
    };

public Q_SLOTS:
    void switchToNextLayout();
    void switchToPreviousLayout();
    bool setLayout(uint index);
    uint getLayout() const;
    QList<LayoutNames> getLayoutsList() const;

Q_SIGNALS:
    void layoutChanged(uint index);
    void layoutListChanged();

private:
    Xkb *m_xkb;
    const KConfigGroup &m_configGroup;
    KeyboardLayout *m_keyboardLayout;
};

QDBusArgument &operator<<(QDBusArgument &argument, const KeyboardLayoutDBusInterface::LayoutNames &layoutNames);
const QDBusArgument &operator>>(const QDBusArgument &argument, KeyboardLayoutDBusInterface::LayoutNames &layoutNames);

}
Q_DECLARE_METATYPE(KWin::KeyboardLayoutDBusInterface::LayoutNames)
