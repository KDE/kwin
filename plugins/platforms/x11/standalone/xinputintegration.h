/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XINPUTINTEGRATION_H
#define KWIN_XINPUTINTEGRATION_H

#include <QObject>
#include <QPointer>
#include <QScopedPointer>
typedef struct _XDisplay Display;

namespace KWin
{

class XInputEventFilter;
class XKeyPressReleaseEventFilter;
class X11Cursor;

class XInputIntegration : public QObject
{
    Q_OBJECT
public:
    explicit XInputIntegration(Display *display, QObject *parent);
    ~XInputIntegration() override;

    void init();
    void startListening();

    bool hasXinput() const {
        return m_hasXInput;
    }
    void setCursor(X11Cursor *cursor);

private:
    Display *display() const {
        return m_x11Display;
    }

    bool m_hasXInput = false;
    int m_xiOpcode = 0;
    int m_majorVersion = 0;
    int m_minorVersion = 0;
    QPointer<X11Cursor> m_x11Cursor;
    Display *m_x11Display;

    QScopedPointer<XInputEventFilter> m_xiEventFilter;
    QScopedPointer<XKeyPressReleaseEventFilter> m_keyPressFilter;
    QScopedPointer<XKeyPressReleaseEventFilter> m_keyReleaseFilter;
};

}

#endif
