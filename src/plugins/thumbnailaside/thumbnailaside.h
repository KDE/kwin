/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 Testing of painting a window more than once.

*/

#pragma once

#include "effect/effect.h"

#include <QHash>

namespace KWin
{

class ThumbnailAsideEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int maxWidth READ configuredMaxWidth)
    Q_PROPERTY(int spacing READ configuredSpacing)
    Q_PROPERTY(qreal opacity READ configuredOpacity)
    Q_PROPERTY(int screen READ configuredScreen)
public:
    ThumbnailAsideEffect();
    void reconfigure(ReconfigureFlags) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    // for properties
    int configuredMaxWidth() const
    {
        return maxwidth;
    }
    int configuredSpacing() const
    {
        return spacing;
    }
    qreal configuredOpacity() const
    {
        return opacity;
    }
    int configuredScreen() const
    {
        return screen;
    }
    bool isActive() const override;

private Q_SLOTS:
    void toggleCurrentThumbnail();
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w, const QRectF &old);
    void slotWindowDamaged(KWin::EffectWindow *w);
    void repaintAll();

private:
    void addThumbnail(EffectWindow *w);
    void removeThumbnail(EffectWindow *w);
    void arrange();
    struct Data
    {
        EffectWindow *window; // the same like the key in the hash (makes code simpler)
        int index;
        QRect rect;
    };
    QHash<EffectWindow *, Data> windows;
    int maxwidth;
    int spacing;
    double opacity;
    int screen;
    QRegion painted;
};

} // namespace
