/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef MOCK_EFFECTS_HANDLER_H
#define MOCK_EFFECTS_HANDLER_H

#include <kwineffects.h>
#include <QX11Info>

class MockEffectsHandler : public KWin::EffectsHandler
{
    Q_OBJECT
public:
    explicit MockEffectsHandler(KWin::CompositingType type);
    void activateWindow(KWin::EffectWindow *) override {}
    KWin::Effect *activeFullScreenEffect() const override {
        return nullptr;
    }
    bool hasActiveFullScreenEffect() const override {
        return false;
    }
    int activeScreen() const override {
        return 0;
    }
    KWin::EffectWindow *activeWindow() const override {
        return nullptr;
    }
    void addRepaint(const QRect &) override {}
    void addRepaint(const QRegion &) override {}
    void addRepaint(int, int, int, int) override {}
    void addRepaintFull() override {}
    double animationTimeFactor() const override {
        return 0;
    }
    xcb_atom_t announceSupportProperty(const QByteArray &, KWin::Effect *) override {
        return XCB_ATOM_NONE;
    }
    void buildQuads(KWin::EffectWindow *, KWin::WindowQuadList &) override {}
    QRect clientArea(KWin::clientAreaOption, const QPoint &, int) const override {
        return QRect();
    }
    QRect clientArea(KWin::clientAreaOption, const KWin::EffectWindow *) const override {
        return QRect();
    }
    QRect clientArea(KWin::clientAreaOption, int, int) const override {
        return QRect();
    }
    void closeTabBox() override {}
    QString currentActivity() const override {
        return QString();
    }
    int currentDesktop() const override {
        return 0;
    }
    int currentTabBoxDesktop() const override {
        return 0;
    }
    QList< int > currentTabBoxDesktopList() const override {
        return QList<int>();
    }
    KWin::EffectWindow *currentTabBoxWindow() const override {
        return nullptr;
    }
    KWin::EffectWindowList currentTabBoxWindowList() const override {
        return KWin::EffectWindowList();
    }
    QPoint cursorPos() const override {
        return QPoint();
    }
    bool decorationsHaveAlpha() const override {
        return false;
    }
    bool decorationSupportsBlurBehind() const override {
        return false;
    }
    void defineCursor(Qt::CursorShape) override {}
    int desktopAbove(int, bool) const override {
        return 0;
    }
    int desktopAtCoords(QPoint) const override {
        return 0;
    }
    int desktopBelow(int, bool) const override {
        return 0;
    }
    QPoint desktopCoords(int) const override {
        return QPoint();
    }
    QPoint desktopGridCoords(int) const override {
        return QPoint();
    }
    int desktopGridHeight() const override {
        return 0;
    }
    QSize desktopGridSize() const override {
        return QSize();
    }
    int desktopGridWidth() const override {
        return 0;
    }
    QString desktopName(int) const override {
        return QString();
    }
    int desktopToLeft(int, bool) const override {
        return 0;
    }
    int desktopToRight(int, bool) const override {
        return 0;
    }
    void doneOpenGLContextCurrent() override {}
    void drawWindow(KWin::EffectWindow *, int, const QRegion &, KWin::WindowPaintData &) override {}
    KWin::EffectFrame *effectFrame(KWin::EffectFrameStyle, bool, const QPoint &, Qt::Alignment) const override {
        return nullptr;
    }
    KWin::EffectWindow *findWindow(WId) const override {
        return nullptr;
    }
    KWin::EffectWindow *findWindow(KWaylandServer::SurfaceInterface *) const override {
        return nullptr;
    }
    KWin::EffectWindow *findWindow(QWindow *w) const override {
        Q_UNUSED(w)
        return nullptr;
    }
    KWin::EffectWindow *findWindow(const QUuid &id) const override {
        Q_UNUSED(id)
        return nullptr;
    }
    void *getProxy(QString) override {
        return nullptr;
    }
    bool grabKeyboard(KWin::Effect *) override {
        return false;
    }
    bool hasDecorationShadows() const override {
        return false;
    }
    bool isScreenLocked() const override {
        return false;
    }
    QVariant kwinOption(KWin::KWinOption) override {
        return QVariant();
    }
    bool makeOpenGLContextCurrent() override {
        return false;
    }
    void moveWindow(KWin::EffectWindow *, const QPoint &, bool, double) override {}
    KWin::WindowQuadType newWindowQuadType() override {
        return KWin::WindowQuadError;
    }
    int numberOfDesktops() const override {
        return 0;
    }
    int numScreens() const override {
        return 0;
    }
    bool optionRollOverDesktops() const override {
        return false;
    }
    void paintEffectFrame(KWin::EffectFrame *, const QRegion &, double, double) override {}
    void paintScreen(int, const QRegion &, KWin::ScreenPaintData &) override {}
    void paintWindow(KWin::EffectWindow *, int, const QRegion &, KWin::WindowPaintData &) override {}
    void postPaintScreen() override {}
    void postPaintWindow(KWin::EffectWindow *) override {}
    void prePaintScreen(KWin::ScreenPrePaintData &, int) override {}
    void prePaintWindow(KWin::EffectWindow *, KWin::WindowPrePaintData &, int) override {}
    QByteArray readRootProperty(long int, long int, int) const override {
        return QByteArray();
    }
    void reconfigure() override {}
    void refTabBox() override {}
    void registerAxisShortcut(Qt::KeyboardModifiers, KWin::PointerAxisDirection, QAction *) override {}
    void registerGlobalShortcut(const QKeySequence &, QAction *) override {}
    void registerPointerShortcut(Qt::KeyboardModifiers, Qt::MouseButton, QAction *) override {}
    void registerTouchpadSwipeShortcut(KWin::SwipeDirection, QAction *) override {}
    void reloadEffect(KWin::Effect *) override {}
    void removeSupportProperty(const QByteArray &, KWin::Effect *) override {}
    void reserveElectricBorder(KWin::ElectricBorder, KWin::Effect *) override {}
    void registerTouchBorder(KWin::ElectricBorder, QAction *) override {}
    void unregisterTouchBorder(KWin::ElectricBorder, QAction *) override {}
    QPainter *scenePainter() override {
        return nullptr;
    }
    int screenNumber(const QPoint &) const override {
        return 0;
    }
    void setActiveFullScreenEffect(KWin::Effect *) override {}
    void setCurrentDesktop(int) override {}
    void setElevatedWindow(KWin::EffectWindow *, bool) override {}
    void setNumberOfDesktops(int) override {}
    void setShowingDesktop(bool) override {}
    void setTabBoxDesktop(int) override {}
    void setTabBoxWindow(KWin::EffectWindow*) override {}
    KWin::EffectWindowList stackingOrder() const override {
        return KWin::EffectWindowList();
    }
    void startMouseInterception(KWin::Effect *, Qt::CursorShape) override {}
    void startMousePolling() override {}
    void stopMouseInterception(KWin::Effect *) override {}
    void stopMousePolling() override {}
    void ungrabKeyboard() override {}
    void unrefTabBox() override {}
    void unreserveElectricBorder(KWin::ElectricBorder, KWin::Effect *) override {}
    QRect virtualScreenGeometry() const override {
        return QRect();
    }
    QSize virtualScreenSize() const override {
        return QSize();
    }
    void windowToDesktop(KWin::EffectWindow *, int) override {}
    void windowToScreen(KWin::EffectWindow *, int) override {}
    int workspaceHeight() const override {
        return 0;
    }
    int workspaceWidth() const override {
        return 0;
    }
    long unsigned int xrenderBufferPicture() override {
        return 0;
    }
    xcb_connection_t *xcbConnection() const override {
        return QX11Info::connection();
    }
    xcb_window_t x11RootWindow() const override {
        return QX11Info::appRootWindow();
    }
    KWaylandServer::Display *waylandDisplay() const override {
        return nullptr;
    }

    bool animationsSupported() const override {
        return m_animationsSuported;
    }
    void setAnimationsSupported(bool set) {
        m_animationsSuported = set;
    }

    KWin::PlatformCursorImage cursorImage() const override {
        return KWin::PlatformCursorImage();
    }

    void hideCursor() override {}

    void showCursor() override {}

    void startInteractiveWindowSelection(std::function<void(KWin::EffectWindow*)> callback) override {
        callback(nullptr);
    }
    void startInteractivePositionSelection(std::function<void (const QPoint &)> callback) override {
        callback(QPoint(-1, -1));
    }
    void showOnScreenMessage(const QString &message, const QString &iconName = QString()) override {
        Q_UNUSED(message)
        Q_UNUSED(iconName)
    }
    void hideOnScreenMessage(OnScreenMessageHideFlags flags = OnScreenMessageHideFlags()) override { Q_UNUSED(flags)}

    void windowToDesktops(KWin::EffectWindow *w, const QVector<uint> &desktops) override {
        Q_UNUSED(w)
        Q_UNUSED(desktops)
    }

    KSharedConfigPtr config() const override;
    KSharedConfigPtr inputConfig() const override;
    void renderEffectQuickView(KWin::EffectQuickView *quickView) const override {
        Q_UNUSED(quickView);
    }
    KWin::SessionState sessionState() const override {
        return KWin::SessionState::Normal;
    }

private:
    bool m_animationsSuported = true;
};
#endif
