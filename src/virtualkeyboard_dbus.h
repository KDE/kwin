/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "inputmethod.h"
#include <QObject>

namespace KWin
{

class KWIN_EXPORT VirtualKeyboardDBus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.VirtualKeyboard")
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool activeClientSupportsTextInput READ activeClientSupportsTextInput NOTIFY activeClientSupportsTextInputChanged)
public:
    explicit VirtualKeyboardDBus(InputMethod *inputMethod);
    ~VirtualKeyboardDBus() override;
    bool isEnabled() const;

    bool activeClientSupportsTextInput() const;
    bool isVisible() const;
    bool isActive() const;
    bool isAvailable() const;
    void setEnabled(bool enabled);
    void setActive(bool active);

    Q_SCRIPTABLE bool willShowOnActive() const;
    Q_SCRIPTABLE void forceActivate();

Q_SIGNALS:
    Q_SCRIPTABLE void enabledChanged();
    Q_SCRIPTABLE void activeChanged();
    Q_SCRIPTABLE void visibleChanged();
    Q_SCRIPTABLE void availableChanged();
    Q_SCRIPTABLE void activeClientSupportsTextInputChanged();

private:
    InputMethod *const m_inputMethod;
};

}
