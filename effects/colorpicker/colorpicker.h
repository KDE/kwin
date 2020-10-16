/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_COLORPICKER_H
#define KWIN_COLORPICKER_H

#include <kwineffects.h>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QColor>

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
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();

public Q_SLOTS:
    Q_SCRIPTABLE QColor pick();

private:
    void showInfoMessage();
    void hideInfoMessage();

    QDBusMessage m_replyMessage;
    QRect m_cachedOutputGeometry;
    QPoint m_scheduledPosition;
    bool m_picking = false;
};

} // namespace

#endif
