/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwineffects.h"
#include "scene/workspacescene.h"

#include <QHash>

#include <memory>

class QMouseEvent;
class QWheelEvent;

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace KWin
{
class Display;
class Window;
class Compositor;
class EffectLoader;
class Unmanaged;
class WindowPropertyNotifyX11Filter;
class TabletEvent;
class TabletPadId;
class TabletToolId;

class KWIN_EXPORT EffectsHandlerImpl : public EffectsHandler
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Effects")
    Q_PROPERTY(QStringList activeEffects READ activeEffects)
    Q_PROPERTY(QStringList loadedEffects READ loadedEffects)
    Q_PROPERTY(QStringList listOfEffects READ listOfEffects)
public:
    EffectsHandlerImpl(Compositor *compositor, WorkspaceScene *scene);
    ~EffectsHandlerImpl() override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    void postPaintWindow(EffectWindow *w) override;

    Effect *provides(Effect::Feature ef);

    void drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    void renderWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;

    void activateWindow(EffectWindow *c) override;
    EffectWindow *activeWindow() const override;
    void moveWindow(EffectWindow *w, const QPoint &pos, bool snap = false, double snapAdjust = 1.0) override;
    void windowToScreen(EffectWindow *w, Output *screen) override;
    void setShowingDesktop(bool showing) override;

    QString currentActivity() const override;
    VirtualDesktop *currentDesktop() const override;
    QList<VirtualDesktop *> desktops() const override;
    void setCurrentDesktop(VirtualDesktop *desktop) override;
    QSize desktopGridSize() const override;
    int desktopGridWidth() const override;
    int desktopGridHeight() const override;
    int workspaceWidth() const override;
    int workspaceHeight() const override;
    VirtualDesktop *desktopAtCoords(QPoint coords) const override;
    QPoint desktopGridCoords(VirtualDesktop *desktop) const override;
    QPoint desktopCoords(VirtualDesktop *desktop) const override;
    VirtualDesktop *desktopAbove(VirtualDesktop *desktop = nullptr, bool wrap = true) const override;
    VirtualDesktop *desktopToRight(VirtualDesktop *desktop = nullptr, bool wrap = true) const override;
    VirtualDesktop *desktopBelow(VirtualDesktop *desktop = nullptr, bool wrap = true) const override;
    VirtualDesktop *desktopToLeft(VirtualDesktop *desktop = nullptr, bool wrap = true) const override;
    QString desktopName(VirtualDesktop *desktop) const override;
    bool optionRollOverDesktops() const override;

    QPointF cursorPos() const override;
    bool grabKeyboard(Effect *effect) override;
    void ungrabKeyboard() override;
    // not performing XGrabPointer
    void startMouseInterception(Effect *effect, Qt::CursorShape shape) override;
    void stopMouseInterception(Effect *effect) override;
    bool isMouseInterception() const;
    void registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action) override;
    void registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action) override;
    void registerTouchpadSwipeShortcut(SwipeDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback) override;
    void registerTouchpadPinchShortcut(PinchDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback) override;
    void registerTouchscreenSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> progressCallback) override;
    void startMousePolling() override;
    void stopMousePolling() override;
    EffectWindow *findWindow(WId id) const override;
    EffectWindow *findWindow(SurfaceInterface *surf) const override;
    EffectWindow *findWindow(QWindow *w) const override;
    EffectWindow *findWindow(const QUuid &id) const override;
    EffectWindowList stackingOrder() const override;
    void setElevatedWindow(KWin::EffectWindow *w, bool set) override;

    void setTabBoxWindow(EffectWindow *) override;
    EffectWindowList currentTabBoxWindowList() const override;
    void refTabBox() override;
    void unrefTabBox() override;
    void closeTabBox() override;
    EffectWindow *currentTabBoxWindow() const override;

    void setActiveFullScreenEffect(Effect *e) override;
    Effect *activeFullScreenEffect() const override;
    bool hasActiveFullScreenEffect() const override;

    void addRepaintFull() override;
    void addRepaint(const QRect &r) override;
    void addRepaint(const QRectF &r) override;

    void addRepaint(const QRegion &r) override;
    void addRepaint(int x, int y, int w, int h) override;
    Output *activeScreen() const override;
    QRectF clientArea(clientAreaOption, const Output *screen, const VirtualDesktop *desktop) const override;
    QRectF clientArea(clientAreaOption, const EffectWindow *c) const override;
    QRectF clientArea(clientAreaOption, const QPoint &p, const VirtualDesktop *desktop) const override;
    QSize virtualScreenSize() const override;
    QRect virtualScreenGeometry() const override;
    double animationTimeFactor() const override;

    void defineCursor(Qt::CursorShape shape) override;
    bool checkInputWindowEvent(QMouseEvent *e);
    bool checkInputWindowEvent(QWheelEvent *e);
    void checkInputWindowStacking();

    void reserveElectricBorder(ElectricBorder border, Effect *effect) override;
    void unreserveElectricBorder(ElectricBorder border, Effect *effect) override;

    void registerTouchBorder(ElectricBorder border, QAction *action) override;
    void registerRealtimeTouchBorder(ElectricBorder border, QAction *action, EffectsHandler::TouchBorderCallback progressCallback) override;
    void unregisterTouchBorder(ElectricBorder border, QAction *action) override;

    QPainter *scenePainter() override;
    void reconfigure() override;
    QByteArray readRootProperty(long atom, long type, int format) const override;
    xcb_atom_t announceSupportProperty(const QByteArray &propertyName, Effect *effect) override;
    void removeSupportProperty(const QByteArray &propertyName, Effect *effect) override;

    bool hasDecorationShadows() const override;

    bool decorationsHaveAlpha() const override;

    QVariant kwinOption(KWinOption kwopt) override;
    bool isScreenLocked() const override;

    bool makeOpenGLContextCurrent() override;
    void doneOpenGLContextCurrent() override;

    xcb_connection_t *xcbConnection() const override;
    xcb_window_t x11RootWindow() const override;

    // internal (used by kwin core or compositing code)
    void startPaint();
    void grabbedKeyboardEvent(QKeyEvent *e);
    bool hasKeyboardGrab() const;

    void reloadEffect(Effect *effect) override;
    QStringList loadedEffects() const;
    QStringList listOfEffects() const;
    void unloadAllEffects();
    QStringList activeEffects() const;
    bool isEffectActive(const QString &pluginId) const;

    /**
     * @returns whether or not any effect is currently active where KWin should not use direct scanout
     */
    bool blocksDirectScanout() const;

    Display *waylandDisplay() const override;

    bool animationsSupported() const override;

    PlatformCursorImage cursorImage() const override;

    void hideCursor() override;
    void showCursor() override;

    void startInteractiveWindowSelection(std::function<void(KWin::EffectWindow *)> callback) override;
    void startInteractivePositionSelection(std::function<void(const QPointF &)> callback) override;

    void showOnScreenMessage(const QString &message, const QString &iconName = QString()) override;
    void hideOnScreenMessage(OnScreenMessageHideFlags flags = OnScreenMessageHideFlags()) override;

    KSharedConfigPtr config() const override;
    KSharedConfigPtr inputConfig() const override;

    WorkspaceScene *scene() const
    {
        return m_scene;
    }

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    bool touchUp(qint32 id, std::chrono::microseconds time);

    bool tabletToolEvent(KWin::TabletEvent *event);
    bool tabletToolButtonEvent(uint button, bool pressed, const KWin::TabletToolId &tabletToolId, std::chrono::microseconds time);
    bool tabletPadButtonEvent(uint button, bool pressed, const KWin::TabletPadId &tabletPadId, std::chrono::microseconds time);
    bool tabletPadStripEvent(int number, int position, bool isFinger, const KWin::TabletPadId &tabletPadId, std::chrono::microseconds time);
    bool tabletPadRingEvent(int number, int position, bool isFinger, const KWin::TabletPadId &tabletPadId, std::chrono::microseconds time);

    void highlightWindows(const QList<EffectWindow *> &windows);

    bool isPropertyTypeRegistered(xcb_atom_t atom) const
    {
        return registered_atoms.contains(atom);
    }

    void windowToDesktops(EffectWindow *w, const QList<VirtualDesktop *> &desktops) override;

    /**
     * Finds an effect with the given name.
     *
     * @param name The name of the effect.
     * @returns The effect with the given name @p name, or nullptr if there
     *     is no such effect loaded.
     */
    Effect *findEffect(const QString &name) const;

    void renderOffscreenQuickView(const RenderTarget &renderTarget, const RenderViewport &viewport, OffscreenQuickView *effectQuickView) const override;

    SessionState sessionState() const override;
    QList<Output *> screens() const override;
    Output *screenAt(const QPoint &point) const override;
    Output *findScreen(const QString &name) const override;
    Output *findScreen(int screenId) const override;
    void renderScreen(Output *screen) override;
    bool isCursorHidden() const override;

    KWin::EffectWindow *inputPanel() const override;
    bool isInputPanelOverlay() const override;

    QQmlEngine *qmlEngine() const override;

public Q_SLOTS:
    // slots for D-Bus interface
    Q_SCRIPTABLE void reconfigureEffect(const QString &name);
    Q_SCRIPTABLE bool loadEffect(const QString &name);
    Q_SCRIPTABLE void toggleEffect(const QString &name);
    Q_SCRIPTABLE void unloadEffect(const QString &name);
    Q_SCRIPTABLE bool isEffectLoaded(const QString &name) const;
    Q_SCRIPTABLE bool isEffectSupported(const QString &name);
    Q_SCRIPTABLE QList<bool> areEffectsSupported(const QStringList &names);
    Q_SCRIPTABLE QString supportInformation(const QString &name) const;
    Q_SCRIPTABLE QString debug(const QString &name, const QString &parameter = QString()) const;

protected:
    void connectNotify(const QMetaMethod &signal) override;
    void disconnectNotify(const QMetaMethod &signal) override;
    void effectsChanged();
    void setupWindowConnections(KWin::Window *window);

    /**
     * Default implementation does nothing and returns @c true.
     */
    virtual bool doGrabKeyboard();
    /**
     * Default implementation does nothing.
     */
    virtual void doUngrabKeyboard();

    /**
     * Default implementation sets Effects override cursor on the PointerInputRedirection.
     */
    virtual void doStartMouseInterception(Qt::CursorShape shape);

    /**
     * Default implementation removes the Effects override cursor on the PointerInputRedirection.
     */
    virtual void doStopMouseInterception();

    /**
     * Default implementation does nothing
     */
    virtual void doCheckInputWindowStacking();

    Effect *keyboard_grab_effect;
    Effect *fullscreen_effect;
    QMultiMap<int, EffectPair> effect_order;
    QHash<long, int> registered_atoms;

private:
    void registerPropertyType(long atom, bool reg);
    void destroyEffect(Effect *effect);
    void reconfigureEffects();

    typedef QList<Effect *> EffectsList;
    typedef EffectsList::const_iterator EffectsIterator;
    EffectsList m_activeEffects;
    EffectsIterator m_currentDrawWindowIterator;
    EffectsIterator m_currentPaintWindowIterator;
    EffectsIterator m_currentPaintScreenIterator;
    typedef QHash<QByteArray, QList<Effect *>> PropertyEffectMap;
    PropertyEffectMap m_propertiesForEffects;
    QHash<QByteArray, qulonglong> m_managedProperties;
    Compositor *m_compositor;
    WorkspaceScene *m_scene;
    QList<Effect *> m_grabbedMouseEffects;
    EffectLoader *m_effectLoader;
    int m_trackingCursorChanges;
    std::unique_ptr<WindowPropertyNotifyX11Filter> m_x11WindowPropertyNotify;
};

inline xcb_window_t EffectsHandlerImpl::x11RootWindow() const
{
    return kwinApp()->x11RootWindow();
}

inline xcb_connection_t *EffectsHandlerImpl::xcbConnection() const
{
    return kwinApp()->x11Connection();
}

} // namespace
