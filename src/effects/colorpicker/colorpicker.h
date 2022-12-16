/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <kwineffects.h>

namespace KWin
{

class ColorPickerEffect : public Effect, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.ColorPicker")
public:
    ColorPickerEffect();
    ~ColorPickerEffect() override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 0;
    }

    static bool supported();

public Q_SLOTS:
    Q_SCRIPTABLE QColor pick();

private:
    void showInfoMessage();
    void hideInfoMessage();

    QDBusMessage m_replyMessage;
    QPoint m_scheduledPosition;
    bool m_picking = false;
};

} // namespace
