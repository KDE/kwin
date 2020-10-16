/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
    ~VirtualKeyboardDBus() override;
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
