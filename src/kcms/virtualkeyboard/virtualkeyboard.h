/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <qqmlregistration.h>

#include "virtualkeyboard_interface.h"

class KWinVirtualKeyboard : public OrgKdeKwinVirtualKeyboardInterface
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool activeClientSupportsTextInput READ activeClientSupportsTextInput NOTIFY activeClientSupportsTextInputChanged)

public:
    enum class VirtualKeyboardMode {
        Off,
        TouchOnly,
        On,
    };
    Q_ENUM(VirtualKeyboardMode);

    KWinVirtualKeyboard();
};
