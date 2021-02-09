/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Thomas Lübking <thomas.luebking@web.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WINDOWGEOMETRY_H
#define WINDOWGEOMETRY_H

#include <kwineffects.h>

namespace KWin
{

class WindowGeometry : public Effect
{
    Q_OBJECT
    Q_PROPERTY(bool handlesMoves READ isHandlesMoves)
    Q_PROPERTY(bool handlesResizes READ isHandlesResizes)
public:
    WindowGeometry();
    ~WindowGeometry() override;

    inline bool provides(Effect::Feature ef) override {
        return ef == Effect::GeometryTip;
    }
    void reconfigure(ReconfigureFlags) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 90;
    }

    // for properties
    bool isHandlesMoves() const {
        return iHandleMoves;
    }
    bool isHandlesResizes() const {
        return iHandleResizes;
    }
private Q_SLOTS:
    void toggle();
    void slotWindowStartUserMovedResized(KWin::EffectWindow *w);
    void slotWindowFinishUserMovedResized(KWin::EffectWindow *w);
    void slotWindowStepUserMovedResized(KWin::EffectWindow *w, const QRect &geometry);
private:
    void createFrames();
    EffectWindow *myResizeWindow;
    EffectFrame *myMeasure[3] = {nullptr, nullptr, nullptr};
    QRect myOriginalGeometry, myCurrentGeometry;
    QRect myExtraDirtyArea;
    bool iAmActive, iAmActivated, iHandleMoves, iHandleResizes;
    QString myCoordString[2], myResizeString;
};

} // namespace

#endif
