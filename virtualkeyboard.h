/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_VIRTUAL_KEYBOARD_H
#define KWIN_VIRTUAL_KEYBOARD_H

#include <QObject>

#include <kwinglobals.h>
#include <kwin_export.h>

#include <abstract_client.h>

class QQuickView;
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
    void hide();
    void show();

Q_SIGNALS:
    void enabledChanged(bool enabled);

private:
    void setEnabled(bool enable);
    void updateSni();
    void updateInputPanelState();
    void adoptInputMethodContext();

    bool m_enabled = false;
    KStatusNotifierItem *m_sni = nullptr;
    QPointer<AbstractClient> m_inputClient;
    QPointer<AbstractClient> m_trackedClient;
    // If a surface loses focus immediately after being resized by the keyboard, don't react to it to avoid resize loops
    QTimer *m_floodTimer;

    QMetaObject::Connection m_waylandShowConnection;
    QMetaObject::Connection m_waylandHideConnection;
    QMetaObject::Connection m_waylandHintsConnection;
    QMetaObject::Connection m_waylandSurroundingTextConnection;
    QMetaObject::Connection m_waylandResetConnection;
    QMetaObject::Connection m_waylandEnabledConnection;
    QMetaObject::Connection m_waylandStateCommittedConnection;

    KWIN_SINGLETON(VirtualKeyboard)
};

}

#endif
