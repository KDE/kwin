/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

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

#ifndef KWIN_SLIDE_H
#define KWIN_SLIDE_H

// KDE
#include <kwineffects.h>

// Qt
#include <QObject>
#include <QTimeLine>

namespace KWin
{

class SlideEffect : public Effect
{
    Q_OBJECT
public:
    SlideEffect();

    void reconfigure(ReconfigureFlags) override;

    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, QRegion region, ScreenPaintData& data) override;
    void postPaintScreen() override;

    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;

    bool isActive() const override {
        return m_active;
    }

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();

private Q_SLOTS:
    void desktopChanged(int old, int current, EffectWindow* with);
    void windowAdded(EffectWindow* w);
    void windowDeleted(EffectWindow* w);

    void numberDesktopsChanged(uint old);
    void numberScreensChanged();

private:
    QPoint desktopCoords(int id) const;
    QRect desktopGeometry(int id) const;
    int workspaceWidth() const;
    int workspaceHeight() const;

    bool isTranslated(const EffectWindow* w) const;
    bool isPainted(const EffectWindow* w) const;

    bool shouldForceBlur(const EffectWindow* w) const;
    bool shouldForceBackgroundContrast(const EffectWindow* w) const;
    bool shouldElevate(const EffectWindow* w) const;

    void start(int old, int current, EffectWindow* movingWindow = nullptr);
    void stop();

private:
    int m_hGap;
    int m_vGap;
    bool m_slideDocks;
    bool m_slideBackground;

    bool m_active = false;
    QTimeLine m_timeline;
    QPoint m_startPos;
    QPoint m_diff;
    EffectWindow* m_movingWindow = nullptr;

    struct {
        int desktop;
        bool firstPass;
        bool lastPass;
        QPoint translation;

        EffectWindowList fullscreenWindows;
    } m_paintCtx;

    struct {
        QList<EffectWindow*> blur;
        QList<EffectWindow*> backgroundContrast;
    } m_forcedRoles;

    QList<EffectWindow*> m_elevatedWindows;
};

} // namespace

#endif
