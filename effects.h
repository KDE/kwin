/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_EFFECTSIMPL_H
#define KWIN_EFFECTSIMPL_H

#include "kwineffects.h"

#include "scene.h"

#include <QHash>
#include <Plasma/FrameSvg>

#include <memory>

namespace Plasma {
class Theme;
}

namespace KWaylandServer
{
class Display;
}

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;


namespace KWin
{

class AbstractThumbnailItem;
class DesktopThumbnailItem;
class WindowThumbnailItem;

class AbstractClient;
class Compositor;
class Deleted;
class EffectLoader;
class Group;
class Toplevel;
class Unmanaged;
class WindowPropertyNotifyX11Filter;

class KWIN_EXPORT EffectsHandlerImpl : public EffectsHandler
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Effects")
    Q_PROPERTY(QStringList activeEffects READ activeEffects)
    Q_PROPERTY(QStringList loadedEffects READ loadedEffects)
    Q_PROPERTY(QStringList listOfEffects READ listOfEffects)
public:
    EffectsHandlerImpl(Compositor *compositor, Scene *scene);
    ~EffectsHandlerImpl() override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    /**
     * Special hook to perform a paintScreen but just with the windows on @p desktop.
     */
    void paintDesktop(int desktop, int mask, QRegion region, ScreenPaintData& data);
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data) override;
    void postPaintWindow(EffectWindow* w) override;
    void paintEffectFrame(EffectFrame* frame, const QRegion &region, double opacity, double frameOpacity) override;

    Effect *provides(Effect::Feature ef);

    void drawWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data) override;

    void buildQuads(EffectWindow* w, WindowQuadList& quadList) override;

    void activateWindow(EffectWindow* c) override;
    EffectWindow* activeWindow() const override;
    void moveWindow(EffectWindow* w, const QPoint& pos, bool snap = false, double snapAdjust = 1.0) override;
    void windowToDesktop(EffectWindow* w, int desktop) override;
    void windowToScreen(EffectWindow* w, int screen) override;
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
    bool grabKeyboard(Effect* effect) override;
    void ungrabKeyboard() override;
    // not performing XGrabPointer
    void startMouseInterception(Effect *effect, Qt::CursorShape shape) override;
    void stopMouseInterception(Effect *effect) override;
    bool isMouseInterception() const;
    void registerGlobalShortcut(const QKeySequence &shortcut, QAction *action) override;
    void registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action) override;
    void registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action) override;
    void registerTouchpadSwipeShortcut(SwipeDirection direction, QAction *action) override;
    void* getProxy(QString name) override;
    void startMousePolling() override;
    void stopMousePolling() override;
    EffectWindow* findWindow(WId id) const override;
    EffectWindow* findWindow(KWaylandServer::SurfaceInterface *surf) const override;
    EffectWindow *findWindow(QWindow *w) const override;
    EffectWindow *findWindow(const QUuid &id) const override;
    EffectWindowList stackingOrder() const override;
    void setElevatedWindow(KWin::EffectWindow* w, bool set) override;

    void setTabBoxWindow(EffectWindow*) override;
    void setTabBoxDesktop(int) override;
    EffectWindowList currentTabBoxWindowList() const override;
    void refTabBox() override;
    void unrefTabBox() override;
    void closeTabBox() override;
    QList< int > currentTabBoxDesktopList() const override;
    int currentTabBoxDesktop() const override;
    EffectWindow* currentTabBoxWindow() const override;

    void setActiveFullScreenEffect(Effect* e) override;
    Effect* activeFullScreenEffect() const override;
    bool hasActiveFullScreenEffect() const override;

    void addRepaintFull() override;
    void addRepaint(const QRect& r) override;
    void addRepaint(const QRegion& r) override;
    void addRepaint(int x, int y, int w, int h) override;
    int activeScreen() const override;
    int numScreens() const override;
    int screenNumber(const QPoint& pos) const override;
    QRect clientArea(clientAreaOption, int screen, int desktop) const override;
    QRect clientArea(clientAreaOption, const EffectWindow* c) const override;
    QRect clientArea(clientAreaOption, const QPoint& p, int desktop) const override;
    QSize virtualScreenSize() const override;
    QRect virtualScreenGeometry() const override;
    double animationTimeFactor() const override;
    WindowQuadType newWindowQuadType() override;

    void defineCursor(Qt::CursorShape shape) override;
    bool checkInputWindowEvent(QMouseEvent *e);
    bool checkInputWindowEvent(QWheelEvent *e);
    void checkInputWindowStacking();

    void reserveElectricBorder(ElectricBorder border, Effect *effect) override;
    void unreserveElectricBorder(ElectricBorder border, Effect *effect) override;

    void registerTouchBorder(ElectricBorder border, QAction *action) override;
    void unregisterTouchBorder(ElectricBorder border, QAction *action) override;

    unsigned long xrenderBufferPicture() override;
    QPainter* scenePainter() override;
    void reconfigure() override;
    QByteArray readRootProperty(long atom, long type, int format) const override;
    xcb_atom_t announceSupportProperty(const QByteArray& propertyName, Effect* effect) override;
    void removeSupportProperty(const QByteArray& propertyName, Effect* effect) override;

    bool hasDecorationShadows() const override;

    bool decorationsHaveAlpha() const override;

    bool decorationSupportsBlurBehind() const override;

    EffectFrame* effectFrame(EffectFrameStyle style, bool staticSize, const QPoint& position, Qt::Alignment alignment) const override;

    QVariant kwinOption(KWinOption kwopt) override;
    bool isScreenLocked() const override;

    bool makeOpenGLContextCurrent() override;
    void doneOpenGLContextCurrent() override;

    xcb_connection_t *xcbConnection() const override;
    xcb_window_t x11RootWindow() const override;

    // internal (used by kwin core or compositing code)
    void startPaint();
    void grabbedKeyboardEvent(QKeyEvent* e);
    bool hasKeyboardGrab() const;
    void desktopResized(const QSize &size);

    void reloadEffect(Effect *effect) override;
    QStringList loadedEffects() const;
    QStringList listOfEffects() const;
    void unloadAllEffects();

    QList<EffectWindow*> elevatedWindows() const;
    QStringList activeEffects() const;

    /**
     * @returns Whether we are currently in a desktop rendering process triggered by paintDesktop hook
     */
    bool isDesktopRendering() const {
        return m_desktopRendering;
    }
    /**
     * @returns the desktop currently being rendered in the paintDesktop hook.
     */
    int currentRenderedDesktop() const {
        return m_currentRenderedDesktop;
    }

    KWaylandServer::Display *waylandDisplay() const override;

    bool animationsSupported() const override;

    PlatformCursorImage cursorImage() const override;

    void hideCursor() override;
    void showCursor() override;

    void startInteractiveWindowSelection(std::function<void(KWin::EffectWindow*)> callback) override;
    void startInteractivePositionSelection(std::function<void(const QPoint &)> callback) override;

    void showOnScreenMessage(const QString &message, const QString &iconName = QString()) override;
    void hideOnScreenMessage(OnScreenMessageHideFlags flags = OnScreenMessageHideFlags()) override;

    KSharedConfigPtr config() const override;
    KSharedConfigPtr inputConfig() const override;

    Scene *scene() const {
        return m_scene;
    }

    bool touchDown(qint32 id, const QPointF &pos, quint32 time);
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time);
    bool touchUp(qint32 id, quint32 time);

    void highlightWindows(const QVector<EffectWindow *> &windows);

    bool isPropertyTypeRegistered(xcb_atom_t atom) const {
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

    void renderEffectQuickView(EffectQuickView *effectQuickView) const override;

    SessionState sessionState() const override;

public Q_SLOTS:
    void slotCurrentTabAboutToChange(EffectWindow* from, EffectWindow* to);
    void slotTabAdded(EffectWindow* from, EffectWindow* to);
    void slotTabRemoved(EffectWindow* c, EffectWindow* newActiveWindow);

    // slots for D-Bus interface
    Q_SCRIPTABLE void reconfigureEffect(const QString& name);
    Q_SCRIPTABLE bool loadEffect(const QString& name);
    Q_SCRIPTABLE void toggleEffect(const QString& name);
    Q_SCRIPTABLE void unloadEffect(const QString& name);
    Q_SCRIPTABLE bool isEffectLoaded(const QString& name) const;
    Q_SCRIPTABLE bool isEffectSupported(const QString& name);
    Q_SCRIPTABLE QList<bool> areEffectsSupported(const QStringList &names);
    Q_SCRIPTABLE QString supportInformation(const QString& name) const;
    Q_SCRIPTABLE QString debug(const QString& name, const QString& parameter = QString()) const;

protected Q_SLOTS:
    void slotClientShown(KWin::Toplevel*);
    void slotUnmanagedShown(KWin::Toplevel*);
    void slotWindowClosed(KWin::Toplevel *c, KWin::Deleted *d);
    void slotClientMaximized(KWin::AbstractClient *c, MaximizeMode maxMode);
    void slotOpacityChanged(KWin::Toplevel *t, qreal oldOpacity);
    void slotClientModalityChanged();
    void slotGeometryShapeChanged(KWin::Toplevel *t, const QRect &old);
    void slotFrameGeometryChanged(Toplevel *toplevel, const QRect &oldGeometry);
    void slotPaddingChanged(KWin::Toplevel *t, const QRect &old);
    void slotWindowDamaged(KWin::Toplevel *t, const QRect& r);

protected:
    void connectNotify(const QMetaMethod &signal) override;
    void disconnectNotify(const QMetaMethod &signal) override;
    void effectsChanged();
    void setupClientConnections(KWin::AbstractClient *client);
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

    Effect* keyboard_grab_effect;
    Effect* fullscreen_effect;
    QList<EffectWindow*> elevated_windows;
    QMultiMap< int, EffectPair > effect_order;
    QHash< long, int > registered_atoms;
    int next_window_quad_type;

private:
    void registerPropertyType(long atom, bool reg);
    void destroyEffect(Effect *effect);

    typedef QVector< Effect*> EffectsList;
    typedef EffectsList::const_iterator EffectsIterator;
    EffectsList m_activeEffects;
    EffectsIterator m_currentDrawWindowIterator;
    EffectsIterator m_currentPaintWindowIterator;
    EffectsIterator m_currentPaintEffectFrameIterator;
    EffectsIterator m_currentPaintScreenIterator;
    EffectsIterator m_currentBuildQuadsIterator;
    typedef QHash< QByteArray, QList< Effect*> > PropertyEffectMap;
    PropertyEffectMap m_propertiesForEffects;
    QHash<QByteArray, qulonglong> m_managedProperties;
    Compositor *m_compositor;
    Scene *m_scene;
    bool m_desktopRendering;
    int m_currentRenderedDesktop;
    QList<Effect*> m_grabbedMouseEffects;
    EffectLoader *m_effectLoader;
    int m_trackingCursorChanges;
    std::unique_ptr<WindowPropertyNotifyX11Filter> m_x11WindowPropertyNotify;
};

class EffectWindowImpl : public EffectWindow
{
    Q_OBJECT
public:
    explicit EffectWindowImpl(Toplevel *toplevel);
    ~EffectWindowImpl() override;

    void enablePainting(int reason) override;
    void disablePainting(int reason) override;
    bool isPaintingEnabled() override;

    void addRepaint(const QRect &r) override;
    void addRepaint(int x, int y, int w, int h) override;
    void addRepaintFull() override;
    void addLayerRepaint(const QRect &r) override;
    void addLayerRepaint(int x, int y, int w, int h) override;

    void refWindow() override;
    void unrefWindow() override;

    const EffectWindowGroup* group() const override;

    bool isDeleted() const override;
    bool isMinimized() const override;
    double opacity() const override;
    bool hasAlpha() const override;

    QStringList activities() const override;
    int desktop() const override;
    QVector<uint> desktops() const override;
    int x() const override;
    int y() const override;
    int width() const override;
    int height() const override;

    QSize basicUnit() const override;
    QRect geometry() const override;
    QRect frameGeometry() const override;
    QRect bufferGeometry() const override;

    QString caption() const override;

    QRect expandedGeometry() const override;
    QRegion shape() const override;
    int screen() const override;
    bool hasOwnShape() const override; // only for shadow effect, for now
    QPoint pos() const override;
    QSize size() const override;
    QRect rect() const override;

    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isUserMove() const override;
    bool isUserResize() const override;
    QRect iconGeometry() const override;

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

    KWaylandServer::SurfaceInterface *surface() const override;
    bool isFullScreen() const override;
    bool isUnresponsive() const override;

    QRect contentsRect() const override;
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

    QRect decorationInnerRect() const override;
    QByteArray readProperty(long atom, long type, int format) const override;
    void deleteProperty(long atom) const override;

    EffectWindow* findModal() override;
    EffectWindow* transientFor() override;
    EffectWindowList mainWindows() const override;

    WindowQuadList buildQuads(bool force = false) const override;

    void minimize() override;
    void unminimize() override;
    void closeWindow() override;

    void referencePreviousWindowPixmap() override;
    void unreferencePreviousWindowPixmap() override;

    QWindow *internalWindow() const override;

    const Toplevel* window() const;
    Toplevel* window();

    void setWindow(Toplevel* w);   // internal
    void setSceneWindow(Scene::Window* w);   // internal
    const Scene::Window* sceneWindow() const; // internal
    Scene::Window* sceneWindow(); // internal

    void elevate(bool elevate);

    void setData(int role, const QVariant &data) override;
    QVariant data(int role) const override;

    void registerThumbnail(AbstractThumbnailItem *item);
    QHash<WindowThumbnailItem*, QPointer<EffectWindowImpl> > const &thumbnails() const {
        return m_thumbnails;
    }
    QList<DesktopThumbnailItem*> const &desktopThumbnails() const {
        return m_desktopThumbnails;
    }

private Q_SLOTS:
    void thumbnailDestroyed(QObject *object);
    void thumbnailTargetChanged();
    void desktopThumbnailDestroyed(QObject *object);
private:
    void insertThumbnail(WindowThumbnailItem *item);
    Toplevel* toplevel;
    Scene::Window* sw; // This one is used only during paint pass.
    QHash<int, QVariant> dataMap;
    QHash<WindowThumbnailItem*, QPointer<EffectWindowImpl> > m_thumbnails;
    QList<DesktopThumbnailItem*> m_desktopThumbnails;
    bool managed = false;
    bool waylandClient;
    bool x11Client;
};

class EffectWindowGroupImpl
    : public EffectWindowGroup
{
public:
    explicit EffectWindowGroupImpl(Group* g);
    EffectWindowList members() const override;
private:
    Group* group;
};

class KWIN_EXPORT EffectFrameImpl
    : public QObject, public EffectFrame
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
    const QFont& font() const override;
    void setFont(const QFont& font) override;
    const QRect& geometry() const override;
    void setGeometry(const QRect& geometry, bool force = false) override;
    const QIcon& icon() const override;
    void setIcon(const QIcon& icon) override;
    const QSize& iconSize() const override;
    void setIconSize(const QSize& size) override;
    void setPosition(const QPoint& point) override;
    const QString& text() const override;
    void setText(const QString& text) override;
    EffectFrameStyle style() const override {
        return m_style;
    };
    Plasma::FrameSvg& frame() {
        return m_frame;
    }
    bool isStatic() const {
        return m_static;
    };
    void finalRender(QRegion region, double opacity, double frameOpacity) const;
    void setShader(GLShader* shader) override {
        m_shader = shader;
    }
    GLShader* shader() const override {
        return m_shader;
    }
    void setSelection(const QRect& selection) override;
    const QRect& selection() const {
        return m_selectionGeometry;
    }
    Plasma::FrameSvg& selectionFrame() {
        return m_selection;
    }
    /**
     * The foreground text color as specified by the default Plasma theme.
     */
    QColor styledTextColor();

private Q_SLOTS:
    void plasmaThemeChanged();

private:
    Q_DISABLE_COPY(EffectFrameImpl)   // As we need to use Qt slots we cannot copy this class
    void align(QRect &geometry);   // positions geometry around m_point respecting m_alignment
    void autoResize(); // Auto-resize if not a static size

    EffectFrameStyle m_style;
    Plasma::FrameSvg m_frame; // TODO: share between all EffectFrames
    Plasma::FrameSvg m_selection;

    // Position
    bool m_static;
    QPoint m_point;
    Qt::Alignment m_alignment;
    QRect m_geometry;

    // Contents
    QString m_text;
    QFont m_font;
    QIcon m_icon;
    QSize m_iconSize;
    QRect m_selectionGeometry;

    Scene::EffectFrame* m_sceneFrame;
    GLShader* m_shader;

    Plasma::Theme *m_theme;
};

inline
QList<EffectWindow*> EffectsHandlerImpl::elevatedWindows() const
{
    if (isScreenLocked())
        return QList<EffectWindow*>();
    return elevated_windows;
}

inline
xcb_window_t EffectsHandlerImpl::x11RootWindow() const
{
    return rootWindow();
}

inline
xcb_connection_t *EffectsHandlerImpl::xcbConnection() const
{
    return connection();
}

inline
EffectWindowGroupImpl::EffectWindowGroupImpl(Group* g)
    : group(g)
{
}

EffectWindow* effectWindow(Toplevel* w);
EffectWindow* effectWindow(Scene::Window* w);

inline
const Scene::Window* EffectWindowImpl::sceneWindow() const
{
    return sw;
}

inline
Scene::Window* EffectWindowImpl::sceneWindow()
{
    return sw;
}

inline
const Toplevel* EffectWindowImpl::window() const
{
    return toplevel;
}

inline
Toplevel* EffectWindowImpl::window()
{
    return toplevel;
}


} // namespace

#endif
