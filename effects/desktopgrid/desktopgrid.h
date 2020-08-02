/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_DESKTOPGRID_H
#define KWIN_DESKTOPGRID_H

#include <kwineffects.h>
#include <QObject>
#include <QTimeLine>

#include "kwineffectquickview.h"

namespace KWin
{

class PresentWindowsEffectProxy;

class DesktopGridEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int zoomDuration READ configuredZoomDuration)
    Q_PROPERTY(int border READ configuredBorder)
    Q_PROPERTY(Qt::Alignment desktopNameAlignment READ configuredDesktopNameAlignment)
    Q_PROPERTY(int layoutMode READ configuredLayoutMode)
    Q_PROPERTY(int customLayoutRows READ configuredCustomLayoutRows)
    Q_PROPERTY(bool usePresentWindows READ isUsePresentWindows)
    // TODO: electric borders
public:
    DesktopGridEffect();
    ~DesktopGridEffect() override;
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void windowInputMouseEvent(QEvent* e) override;
    void grabbedKeyboardEvent(QKeyEvent* e) override;
    bool borderActivated(ElectricBorder border) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    enum { LayoutPager, LayoutAutomatic, LayoutCustom }; // Layout modes

    // for properties
    int configuredZoomDuration() const {
        return zoomDuration;
    }
    int configuredBorder() const {
        return border;
    }
    Qt::Alignment configuredDesktopNameAlignment() const {
        return desktopNameAlignment;
    }
    int configuredLayoutMode() const {
        return layoutMode;
    }
    int configuredCustomLayoutRows() const {
        return customLayoutRows;
    }
    bool isUsePresentWindows() const {
        return m_usePresentWindows;
    }
private Q_SLOTS:
    void toggle();
    // slots for global shortcut changed
    // needed to toggle the effect
    void globalShortcutChanged(QAction *action, const QKeySequence& seq);
    void slotAddDesktop();
    void slotRemoveDesktop();
    void slotWindowAdded(KWin::EffectWindow* w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotNumberDesktopsChanged(uint old);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w, const QRect &old);

private:
    QPointF scalePos(const QPoint& pos, int desktop, int screen = -1) const;
    QPoint unscalePos(const QPoint& pos, int* desktop = nullptr) const;
    int posToDesktop(const QPoint& pos) const;
    EffectWindow* windowAt(QPoint pos) const;
    void setCurrentDesktop(int desktop);
    void setHighlightedDesktop(int desktop);
    int desktopToRight(int desktop, bool wrap = true) const;
    int desktopToLeft(int desktop, bool wrap = true) const;
    int desktopUp(int desktop, bool wrap = true) const;
    int desktopDown(int desktop, bool wrap = true) const;
    void setActive(bool active);
    void setup();
    void setupGrid();
    void finish();
    bool isMotionManagerMovingWindows() const;
    bool isRelevantWithPresentWindows(EffectWindow *w) const;
    bool isUsingPresentWindows() const;
    QRectF moveGeometryToDesktop(int desktop) const;
    void desktopsAdded(int old);
    void desktopsRemoved(int old);
    QVector<int> desktopList(const EffectWindow *w) const;

    QList<ElectricBorder> borderActivate;
    int zoomDuration;
    int border;
    Qt::Alignment desktopNameAlignment;
    int layoutMode;
    int customLayoutRows;

    bool activated;
    QTimeLine timeline;
    int paintingDesktop;
    int highlightedDesktop;
    int sourceDesktop;
    int m_originalMovingDesktop;
    bool keyboardGrab;
    bool wasWindowMove, wasWindowCopy, wasDesktopMove, isValidMove;
    EffectWindow* windowMove;
    QPoint windowMoveDiff;
    QPoint dragStartPos;

    // Soft highlighting
    QList<QTimeLine*> hoverTimeline;

    QList< EffectFrame* > desktopNames;

    QSize gridSize;
    Qt::Orientation orientation;
    QPoint activeCell;
    // Per screen variables
    QList<double> scale; // Because the border isn't a ratio each screen is different
    QList<double> unscaledBorder;
    QList<QSizeF> scaledSize;
    QList<QPointF> scaledOffset;

    // Shortcut - needed to toggle the effect
    QList<QKeySequence> shortcut;

    PresentWindowsEffectProxy* m_proxy;
    QList<WindowMotionManager> m_managers;
    bool m_usePresentWindows;
    QRect m_windowMoveGeometry;
    QPoint m_windowMoveStartPoint;

    QVector<EffectQuickScene*> m_desktopButtons;

    QAction *m_activateAction;

};

} // namespace

#endif
