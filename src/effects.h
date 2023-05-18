/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwineffects.h"

#include "kwinoffscreenquickview.h"
#include "scene/workspacescene.h"

#include <QFont>
#include <QHash>

#include <memory>

class QMouseEvent;
class QWheelEvent;

namespace KWaylandServer
{
class Display;
}

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace KWin
{
class Window;
class Compositor;
class Deleted;
class EffectLoader;
class Group;
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
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    void postPaintWindow(EffectWindow *w) override;

    Effect *provides(Effect::Feature ef);

    void drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    void renderWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;

    void activateWindow(EffectWindow *c) override;
    EffectWindow *activeWindow() const override;
    void moveWindow(EffectWindow *w, const QPoint &pos, bool snap = false, double snapAdjust = 1.0) override;
    void windowToDesktop(EffectWindow *w, int desktop) override;
    void windowToScreen(EffectWindow *w, EffectScreen *screen) override;
    void setShowingDesktop(bool showing) override;

    QString currentActivity() const override;
    int currentDesktop() const override;
    int numberOfDesktops() const override;
    void setCurrentDesktop(int desktop) override;
    void setNumberOfDesktops(int desktops) override;
    QSize desktopGridSize() const override;
    int desktopGridWidth() const override;
    int desktopGridHeight() const override;
    int workspaceWidth() const override;
    int workspaceHeight() const override;
    int desktopAtCoords(QPoint coords) const override;
    QPoint desktopGridCoords(int id) const override;
    QPoint desktopCoords(int id) const override;
    int desktopAbove(int desktop = 0, bool wrap = true) const override;
    int desktopToRight(int desktop = 0, bool wrap = true) const override;
    int desktopBelow(int desktop = 0, bool wrap = true) const override;
    int desktopToLeft(int desktop = 0, bool wrap = true) const override;
    QString desktopName(int desktop) const override;
    bool optionRollOverDesktops() const override;

    QPoint cursorPos() const override;
    bool grabKeyboard(Effect *effect) override;
    void ungrabKeyboard() override;
    // not performing XGrabPointer
    void startMouseInterception(Effect *effect, Qt::CursorShape shape) override;
    void stopMouseInterception(Effect *effect) override;
    bool isMouseInterception() const;
    void registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action) override;
    void registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action) override;
    void registerRealtimeTouchpadSwipeShortcut(SwipeDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback) override;
    void registerTouchpadSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action) override;
    void registerRealtimeTouchpadPinchShortcut(PinchDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback) override;
    void registerTouchpadPinchShortcut(PinchDirection direction, uint fingerCount, QAction *action) override;
    void registerTouchscreenSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> progressCallback) override;
    void *getProxy(QString name) override;
    void startMousePolling() override;
    void stopMousePolling() override;
    EffectWindow *findWindow(WId id) const override;
    EffectWindow *findWindow(KWaylandServer::SurfaceInterface *surf) const override;
    EffectWindow *findWindow(QWindow *w) const override;
    EffectWindow *findWindow(const QUuid &id) const override;
    EffectWindowList stackingOrder() const override;
    void setElevatedWindow(KWin::EffectWindow *w, bool set) override;

    void setTabBoxWindow(EffectWindow *) override;
    void setTabBoxDesktop(int) override;
    EffectWindowList currentTabBoxWindowList() const override;
    void refTabBox() override;
    void unrefTabBox() override;
    void closeTabBox() override;
    QList<int> currentTabBoxDesktopList() const override;
    int currentTabBoxDesktop() const override;
    EffectWindow *currentTabBoxWindow() const override;

    void setActiveFullScreenEffect(Effect *e) override;
    Effect *activeFullScreenEffect() const override;
    bool hasActiveFullScreenEffect() const override;

    void addRepaintFull() override;
    void addRepaint(const QRect &r) override;
    void addRepaint(const QRectF &r) override;

    void addRepaint(const QRegion &r) override;
    void addRepaint(int x, int y, int w, int h) override;
    EffectScreen *activeScreen() const override;
    QRectF clientArea(clientAreaOption, const EffectScreen *screen, int desktop) const override;
    QRectF clientArea(clientAreaOption, const EffectWindow *c) const override;
    QRectF clientArea(clientAreaOption, const QPoint &p, int desktop) const override;
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

    std::unique_ptr<EffectFrame> effectFrame(EffectFrameStyle style, bool staticSize, const QPoint &position, Qt::Alignment alignment) const override;

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

    QList<EffectWindow *> elevatedWindows() const;
    QStringList activeEffects() const;

    /**
     * @returns whether or not any effect is currently active where KWin should not use direct scanout
     */
    bool blocksDirectScanout() const;

    KWaylandServer::Display *waylandDisplay() const override;

    bool animationsSupported() const override;

    PlatformCursorImage cursorImage() const override;

    void hideCursor() override;
    void showCursor() override;

    void startInteractiveWindowSelection(std::function<void(KWin::EffectWindow *)> callback) override;
    void startInteractivePositionSelection(std::function<void(const QPoint &)> callback) override;

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

    void highlightWindows(const QVector<EffectWindow *> &windows);

    bool isPropertyTypeRegistered(xcb_atom_t atom) const
    {
        return registered_atoms.contains(atom);
    }

    void windowToDesktops(EffectWindow *w, const QVector<uint> &desktops) override;

    /**
     * Finds an effect with the given name.
     *
     * @param name The name of the effect.
     * @returns The effect with the given name @p name, or nullptr if there
     *     is no such effect loaded.
     */
    Effect *findEffect(const QString &name) const;

    void renderOffscreenQuickView(OffscreenQuickView *effectQuickView) const override;

    SessionState sessionState() const override;
    QList<EffectScreen *> screens() const override;
    EffectScreen *screenAt(const QPoint &point) const override;
    EffectScreen *findScreen(const QString &name) const override;
    EffectScreen *findScreen(int screenId) const override;
    void renderScreen(EffectScreen *screen) override;
    bool isCursorHidden() const override;
    QRect renderTargetRect() const override;
    qreal renderTargetScale() const override;

    KWin::EffectWindow *inputPanel() const override;
    bool isInputPanelOverlay() const override;

public Q_SLOTS:
    void slotCurrentTabAboutToChange(EffectWindow *from, EffectWindow *to);
    void slotTabAdded(EffectWindow *from, EffectWindow *to);
    void slotTabRemoved(EffectWindow *c, EffectWindow *newActiveWindow);

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

protected Q_SLOTS:
    void slotWindowShown(KWin::Window *);
    void slotUnmanagedShown(KWin::Window *);
    void slotWindowClosed(KWin::Window *original, KWin::Deleted *d);
    void slotClientMaximized(KWin::Window *window, MaximizeMode maxMode);
    void slotOpacityChanged(KWin::Window *window, qreal oldOpacity);
    void slotClientModalityChanged();
    void slotGeometryShapeChanged(KWin::Window *window, const QRectF &old);
    void slotFrameGeometryChanged(Window *window, const QRectF &oldGeometry);
    void slotWindowDamaged(KWin::Window *window);
    void slotOutputAdded(Output *output);
    void slotOutputRemoved(Output *output);

protected:
    void connectNotify(const QMetaMethod &signal) override;
    void disconnectNotify(const QMetaMethod &signal) override;
    void effectsChanged();
    void setupWindowConnections(KWin::Window *window);
    void setupUnmanagedConnections(KWin::Unmanaged *u);

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
    QList<EffectWindow *> elevated_windows;
    QMultiMap<int, EffectPair> effect_order;
    QHash<long, int> registered_atoms;

private:
    void registerPropertyType(long atom, bool reg);
    void destroyEffect(Effect *effect);

    typedef QVector<Effect *> EffectsList;
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
    QList<EffectScreen *> m_effectScreens;
};

class EffectScreenImpl : public EffectScreen
{
    Q_OBJECT

public:
    explicit EffectScreenImpl(Output *output, QObject *parent = nullptr);
    ~EffectScreenImpl() override;

    Output *platformOutput() const;

    QString name() const override;
    QString manufacturer() const override;
    QString model() const override;
    QString serialNumber() const override;
    qreal devicePixelRatio() const override;
    QRect geometry() const override;
    int refreshRate() const override;
    Transform transform() const override;

    static EffectScreenImpl *get(Output *output);

private:
    QPointer<Output> m_platformOutput;
};

class EffectWindowImpl : public EffectWindow
{
    Q_OBJECT
public:
    explicit EffectWindowImpl(Window *window);
    ~EffectWindowImpl() override;

    void addRepaint(const QRect &r) override;
    void addRepaintFull() override;
    void addLayerRepaint(const QRect &r) override;

    void refWindow() override;
    void unrefWindow() override;

    const EffectWindowGroup *group() const override;

    bool isDeleted() const override;
    bool isMinimized() const override;
    bool isHidden() const override;
    double opacity() const override;

    QStringList activities() const override;
    int desktop() const override;
    QVector<uint> desktops() const override;
    qreal x() const override;
    qreal y() const override;
    qreal width() const override;
    qreal height() const override;

    QSizeF basicUnit() const override;
    QRectF geometry() const override;
    QRectF frameGeometry() const override;
    QRectF bufferGeometry() const override;
    QRectF clientGeometry() const override;

    QString caption() const override;

    QRectF expandedGeometry() const override;
    EffectScreen *screen() const override;
    QPointF pos() const override;
    QSizeF size() const override;
    QRectF rect() const override;

    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isUserMove() const override;
    bool isUserResize() const override;
    QRectF iconGeometry() const override;

    bool isDesktop() const override;
    bool isDock() const override;
    bool isToolbar() const override;
    bool isMenu() const override;
    bool isNormalWindow() const override;
    bool isSpecialWindow() const override;
    bool isDialog() const override;
    bool isSplash() const override;
    bool isUtility() const override;
    bool isDropdownMenu() const override;
    bool isPopupMenu() const override;
    bool isTooltip() const override;
    bool isNotification() const override;
    bool isCriticalNotification() const override;
    bool isAppletPopup() const override;
    bool isOnScreenDisplay() const override;
    bool isComboBox() const override;
    bool isDNDIcon() const override;
    bool skipsCloseAnimation() const override;

    bool acceptsFocus() const override;
    bool keepAbove() const override;
    bool keepBelow() const override;
    bool isModal() const override;
    bool isPopupWindow() const override;
    bool isOutline() const override;
    bool isLockScreen() const override;

    KWaylandServer::SurfaceInterface *surface() const override;
    bool isFullScreen() const override;
    bool isUnresponsive() const override;

    QRectF contentsRect() const override;
    bool decorationHasAlpha() const override;
    QIcon icon() const override;
    QString windowClass() const override;
    NET::WindowType windowType() const override;
    bool isSkipSwitcher() const override;
    bool isCurrentTab() const override;
    QString windowRole() const override;

    bool isManaged() const override;
    bool isWaylandClient() const override;
    bool isX11Client() const override;

    pid_t pid() const override;
    qlonglong windowId() const override;
    QUuid internalId() const override;

    QRectF decorationInnerRect() const override;
    KDecoration2::Decoration *decoration() const override;
    QByteArray readProperty(long atom, long type, int format) const override;
    void deleteProperty(long atom) const override;

    EffectWindow *findModal() override;
    EffectWindow *transientFor() override;
    EffectWindowList mainWindows() const override;

    void minimize() override;
    void unminimize() override;
    void closeWindow() override;

    void referencePreviousWindowPixmap() override;
    void unreferencePreviousWindowPixmap() override;

    QWindow *internalWindow() const override;

    const Window *window() const;
    Window *window();

    void setWindow(Window *w); // internal
    void setWindowItem(WindowItem *item); // internal
    WindowItem *windowItem() const; // internal

    void elevate(bool elevate);

    void setData(int role, const QVariant &data) override;
    QVariant data(int role) const override;

private:
    void refVisible(const EffectWindowVisibleRef *holder) override;
    void unrefVisible(const EffectWindowVisibleRef *holder) override;

    Window *m_window;
    WindowItem *m_windowItem; // This one is used only during paint pass.
    QHash<int, QVariant> dataMap;
    bool managed = false;
    bool m_waylandWindow;
    bool m_x11Window;
};

class EffectWindowGroupImpl
    : public EffectWindowGroup
{
public:
    explicit EffectWindowGroupImpl(Group *g);
    EffectWindowList members() const override;

private:
    Group *group;
};

class EffectFrameQuickScene : public OffscreenQuickScene
{
    Q_OBJECT

    Q_PROPERTY(QFont font READ font NOTIFY fontChanged)
    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(QSize iconSize READ iconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(qreal frameOpacity READ frameOpacity NOTIFY frameOpacityChanged)
    Q_PROPERTY(bool crossFadeEnabled READ crossFadeEnabled NOTIFY crossFadeEnabledChanged)
    Q_PROPERTY(qreal crossFadeProgress READ crossFadeProgress NOTIFY crossFadeProgressChanged)

public:
    EffectFrameQuickScene(EffectFrameStyle style, bool staticSize, QPoint position,
                          Qt::Alignment alignment, QObject *parent = nullptr);
    ~EffectFrameQuickScene() override;

    EffectFrameStyle style() const;
    bool isStatic() const;

    // has to be const-ref to match EffectFrameImpl...
    const QFont &font() const;
    void setFont(const QFont &font);
    Q_SIGNAL void fontChanged(const QFont &font);

    const QIcon &icon() const;
    void setIcon(const QIcon &icon);
    Q_SIGNAL void iconChanged(const QIcon &icon);

    const QSize &iconSize() const;
    void setIconSize(const QSize &iconSize);
    Q_SIGNAL void iconSizeChanged(const QSize &iconSize);

    const QString &text() const;
    void setText(const QString &text);
    Q_SIGNAL void textChanged(const QString &text);

    qreal frameOpacity() const;
    void setFrameOpacity(qreal frameOpacity);
    Q_SIGNAL void frameOpacityChanged(qreal frameOpacity);

    bool crossFadeEnabled() const;
    void setCrossFadeEnabled(bool enabled);
    Q_SIGNAL void crossFadeEnabledChanged(bool enabled);

    qreal crossFadeProgress() const;
    void setCrossFadeProgress(qreal progress);
    Q_SIGNAL void crossFadeProgressChanged(qreal progress);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    QPoint position() const;
    void setPosition(const QPoint &point);

private:
    void reposition();

    EffectFrameStyle m_style;

    // Position
    bool m_static;
    QPoint m_point;
    Qt::Alignment m_alignment;

    // Contents
    QFont m_font;
    QIcon m_icon;
    QSize m_iconSize;
    QString m_text;
    qreal m_frameOpacity = 0.0;
    bool m_crossFadeEnabled = false;
    qreal m_crossFadeProgress = 0.0;
};

class KWIN_EXPORT EffectFrameImpl
    : public QObject,
      public EffectFrame
{
    Q_OBJECT
public:
    explicit EffectFrameImpl(EffectFrameStyle style, bool staticSize = true, QPoint position = QPoint(-1, -1),
                             Qt::Alignment alignment = Qt::AlignCenter);
    ~EffectFrameImpl() override;

    void free() override;
    void render(const QRegion &region = infiniteRegion(), double opacity = 1.0, double frameOpacity = 1.0) override;
    Qt::Alignment alignment() const override;
    void setAlignment(Qt::Alignment alignment) override;
    const QFont &font() const override;
    void setFont(const QFont &font) override;
    const QRect &geometry() const override;
    void setGeometry(const QRect &geometry, bool force = false) override;
    const QIcon &icon() const override;
    void setIcon(const QIcon &icon) override;
    const QSize &iconSize() const override;
    void setIconSize(const QSize &size) override;
    void setPosition(const QPoint &point) override;
    const QString &text() const override;
    void setText(const QString &text) override;
    EffectFrameStyle style() const override;
    bool isCrossFade() const override;
    void enableCrossFade(bool enable) override;
    qreal crossFadeProgress() const override;
    void setCrossFadeProgress(qreal progress) override;

private:
    Q_DISABLE_COPY(EffectFrameImpl) // As we need to use Qt slots we cannot copy this class

    EffectFrameQuickScene *m_view;
    QRect m_geometry;
};

inline QList<EffectWindow *> EffectsHandlerImpl::elevatedWindows() const
{
    if (isScreenLocked()) {
        return QList<EffectWindow *>();
    }
    return elevated_windows;
}

inline xcb_window_t EffectsHandlerImpl::x11RootWindow() const
{
    return kwinApp()->x11RootWindow();
}

inline xcb_connection_t *EffectsHandlerImpl::xcbConnection() const
{
    return kwinApp()->x11Connection();
}

inline EffectWindowGroupImpl::EffectWindowGroupImpl(Group *g)
    : group(g)
{
}

inline WindowItem *EffectWindowImpl::windowItem() const
{
    return m_windowItem;
}

inline const Window *EffectWindowImpl::window() const
{
    return m_window;
}

inline Window *EffectWindowImpl::window()
{
    return m_window;
}

} // namespace
