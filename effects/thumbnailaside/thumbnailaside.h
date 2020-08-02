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

#ifndef KWIN_THUMBNAILASIDE_H
#define KWIN_THUMBNAILASIDE_H

#include <kwineffects.h>

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
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    // for properties
    int configuredMaxWidth() const {
        return maxwidth;
    }
    int configuredSpacing() const {
        return spacing;
    }
    qreal configuredOpacity() const {
        return opacity;
    }
    int configuredScreen() const {
        return screen;
    }
private Q_SLOTS:
    void toggleCurrentThumbnail();
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w, const QRect &old);
    void slotWindowDamaged(KWin::EffectWindow* w, const QRect& damage);
    bool isActive() const override;
    void repaintAll();
private:
    void addThumbnail(EffectWindow* w);
    void removeThumbnail(EffectWindow* w);
    void arrange();
    struct Data {
        EffectWindow* window; // the same like the key in the hash (makes code simpler)
        int index;
        QRect rect;
    };
    QHash< EffectWindow*, Data > windows;
    int maxwidth;
    int spacing;
    double opacity;
    int screen;
    QRegion painted;
};

} // namespace

#endif
