/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include "inputmethod.h"

namespace KWin
{

class KWIN_EXPORT VirtualKeyboardDBus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.VirtualKeyboard")
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
public:
    explicit VirtualKeyboardDBus(InputMethod *inputMethod);
    ~VirtualKeyboardDBus() override;
    bool isEnabled() const;

    bool isActive() const;
    void setEnabled(bool enabled);
    void setActive(bool active);

Q_SIGNALS:
    Q_SCRIPTABLE void enabledChanged();
    Q_SCRIPTABLE void activeChanged();

private:
    InputMethod *const m_inputMethod;
};

}
