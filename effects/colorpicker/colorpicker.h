/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
