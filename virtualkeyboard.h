/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_VIRTUAL_KEYBOARD_H
#define KWIN_VIRTUAL_KEYBOARD_H

#include <QObject>

#include <kwinglobals.h>
#include <kwin_export.h>

#include <abstract_client.h>

class QQuickWindow;
class QTimer;
class QWindow;
class KStatusNotifierItem;

namespace KWin
{

class KWIN_EXPORT VirtualKeyboard : public QObject
{
    Q_OBJECT
public:
    ~VirtualKeyboard() override;

    void init();

    bool event(QEvent *e) override;
    bool eventFilter(QObject *o, QEvent *event) override;

    QWindow *inputPanel() const;

Q_SIGNALS:
    void enabledChanged(bool enabled);

private:
    void show();
    void hide();
    void setEnabled(bool enable);
    void updateSni();
    void updateInputPanelState();

    bool m_enabled = false;
    KStatusNotifierItem *m_sni = nullptr;
    QScopedPointer<QQuickWindow> m_inputWindow;
    QPointer<AbstractClient> m_trackedClient;
    // If a surface loses focus immediately after being resized by the keyboard, don't react to it to avoid resize loops
    QTimer *m_floodTimer;

    QMetaObject::Connection m_waylandShowConnection;
    QMetaObject::Connection m_waylandHideConnection;
    QMetaObject::Connection m_waylandHintsConnection;
    QMetaObject::Connection m_waylandSurroundingTextConnection;
    QMetaObject::Connection m_waylandResetConnection;
    QMetaObject::Connection m_waylandEnabledConnection;
    KWIN_SINGLETON(VirtualKeyboard)
};

}

#endif
