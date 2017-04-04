/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_KEYBOARD_LAYOUT_H
#define KWIN_KEYBOARD_LAYOUT_H

#include "input_event_spy.h"
#include <QObject>
#include <QVector>

#include <KSharedConfig>
typedef uint32_t xkb_layout_index_t;

class KStatusNotifierItem;
class QAction;

namespace KWin
{
class Xkb;
class KeyboardLayoutDBusInterface;

namespace KeyboardLayoutSwitching
{
class Policy;
}

class KeyboardLayout : public QObject, public InputEventSpy
{
    Q_OBJECT
public:
    explicit KeyboardLayout(Xkb *xkb);
    ~KeyboardLayout() override;

    void setConfig(KSharedConfigPtr config) {
        m_config = config;
    }

    void init();

    void checkLayoutChange();
    void resetLayout();

    void keyEvent(KeyEvent *event) override;

Q_SIGNALS:
    void layoutChanged();
    void layoutsReconfigured();

private Q_SLOTS:
    void reconfigure();

private:
    void initDBusInterface();
    void notifyLayoutChange();
    void initNotifierItem();
    void switchToNextLayout();
    void switchToPreviousLayout();
    void switchToLayout(xkb_layout_index_t index);
    void updateNotifier();
    void reinitNotifierMenu();
    void loadShortcuts();
    Xkb *m_xkb;
    xkb_layout_index_t m_layout = 0;
    KStatusNotifierItem *m_notifierItem;
    KSharedConfigPtr m_config;
    QVector<QAction*> m_layoutShortcuts;
    KeyboardLayoutDBusInterface *m_dbusInterface = nullptr;
    KeyboardLayoutSwitching::Policy *m_policy = nullptr;
};

class KeyboardLayoutDBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KeyboardLayouts")

public:
    explicit KeyboardLayoutDBusInterface(Xkb *xkb, KeyboardLayout *parent);
    ~KeyboardLayoutDBusInterface() override;

public Q_SLOTS:
    bool setLayout(const QString &layout);
    QString getCurrentLayout();
    QStringList getLayoutsList();
    QString getLayoutDisplayName(const QString &layout);

Q_SIGNALS:
    void currentLayoutChanged(QString layout);
    void layoutListChanged();

private:
    Xkb *m_xkb;
    KeyboardLayout *m_keyboardLayout;
};

}

#endif
