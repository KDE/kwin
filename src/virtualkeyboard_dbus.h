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
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit VirtualKeyboardDBus(QObject *parent = nullptr);
    ~VirtualKeyboardDBus() override;
    bool isEnabled() const;
    void setEnabled(bool enabled);

    void setActive(bool active);
    bool isActive() const;

public Q_SLOTS:
    void requestEnabled(bool enabled);
    void hide();

Q_SIGNALS:
    Q_SCRIPTABLE void enabledChanged();
    Q_SCRIPTABLE void activeChanged();
    void hideRequested();
    void enableRequested(bool requested);

private:
    bool m_enabled = false;
    bool m_active = false;
};

}
