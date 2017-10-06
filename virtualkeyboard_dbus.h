/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#pragma once

#include <QObject>

namespace KWin
{

class VirtualKeyboardDBus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.VirtualKeyboard")
    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)
public:
    explicit VirtualKeyboardDBus(QObject *parent = nullptr);
    ~VirtualKeyboardDBus();
    Q_INVOKABLE bool isEnabled() const {
        return m_enabled;
    }
    void setEnabled(bool enabled) {
        if (m_enabled == enabled) {
            return;
        }
        m_enabled = enabled;
        emit enabledChanged();
    }

public Q_SLOTS:
    void enable() {
        emit activateRequested(true);
    }
    void disable() {
        emit activateRequested(false);
    }

Q_SIGNALS:
    Q_SCRIPTABLE void enabledChanged();
    void activateRequested(bool requested);

private:
    bool m_enabled = false;
};

}
