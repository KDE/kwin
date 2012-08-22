/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_EFFECTSIMPL_H
#define KWIN_EFFECTSIMPL_H

#include "kwineffects.h"

#include "scene.h"

#include <QStack>
#include <QHash>
#include <Plasma/FrameSvg>


class KService;


namespace KWin
{

class ThumbnailItem;

class Client;
class Deleted;
class Unmanaged;

class EffectsHandlerImpl : public EffectsHandler
{
    Q_OBJECT
public:
    EffectsHandlerImpl(CompositingType type);
    virtual ~EffectsHandlerImpl();
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintWindow(EffectWindow* w);
    virtual void paintEffectFrame(EffectFrame* frame, QRegion region, double opacity, double frameOpacity);

    Effect *provides(Effect::Feature ef);

    virtual void drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);

    virtual void buildQuads(EffectWindow* w, WindowQuadList& quadList);

    virtual void activateWindow(EffectWindow* c);
    virtual EffectWindow* activeWindow() const;
    virtual void moveWindow(EffectWindow* w, const QPoint& pos, bool snap = false, double snapAdjust = 1.0);
    virtual void windowToDesktop(EffectWindow* w, int desktop);
    virtual void windowToScreen(EffectWindow* w, int screen);
    virtual void setShowingDesktop(bool showing);

    virtual QString currentActivity() const;
    virtual int currentDesktop() const;
    virtual int numberOfDesktops() const;
    virtual void setCurrentDesktop(int desktop);
    virtual void setNumberOfDesktops(int desktops);
    virtual QSize desktopGridSize() const;
    virtual int desktopGridWidth() const;
    virtual int desktopGridHeight() const;
    virtual int workspaceWidth() const;
    virtual int workspaceHeight() const;
    virtual int desktopAtCoords(QPoint coords) const;
    virtual QPoint desktopGridCoords(int id) const;
    virtual QPoint desktopCoords(int id) const;
    virtual int desktopAbove(int desktop = 0, bool wrap = true) const;
    virtual int desktopToRight(int desktop = 0, bool wrap = true) const;
    virtual int desktopBelow(int desktop = 0, bool wrap = true) const;
    virtual int desktopToLeft(int desktop = 0, bool wrap = true) const;
    virtual QString desktopName(int desktop) const;
    virtual bool optionRollOverDesktops() const;

    virtual int displayWidth() const;
    virtual int displayHeight() const;
    virtual QPoint cursorPos() const;
    virtual bool grabKeyboard(Effect* effect);
    virtual void ungrabKeyboard();
    virtual void* getProxy(QString name);
    virtual void startMousePolling();
    virtual void stopMousePolling();
    virtual EffectWindow* findWindow(WId id) const;
    virtual EffectWindowList stackingOrder() const;
    virtual void setElevatedWindow(EffectWindow* w, bool set);

    virtual void setTabBoxWindow(EffectWindow*);
    virtual void setTabBoxDesktop(int);
    virtual EffectWindowList currentTabBoxWindowList() const;
    virtual void refTabBox();
    virtual void unrefTabBox();
    virtual void closeTabBox();
    virtual QList< int > currentTabBoxDesktopList() const;
    virtual int currentTabBoxDesktop() const;
    virtual EffectWindow* currentTabBoxWindow() const;

    virtual void setActiveFullScreenEffect(Effect* e);
    virtual Effect* activeFullScreenEffect() const;

    virtual void addRepaintFull();
    virtual void addRepaint(const QRect& r);
    virtual void addRepaint(const QRegion& r);
    virtual void addRepaint(int x, int y, int w, int h);
    virtual int activeScreen() const;
    virtual int numScreens() const;
    virtual int screenNumber(const QPoint& pos) const;
    virtual QRect clientArea(clientAreaOption, int screen, int desktop) const;
    virtual QRect clientArea(clientAreaOption, const EffectWindow* c) const;
    virtual QRect clientArea(clientAreaOption, const QPoint& p, int desktop) const;
    virtual double animationTimeFactor() const;
    virtual WindowQuadType newWindowQuadType();

    virtual Window createInputWindow(Effect* e, int x, int y, int w, int h, const QCursor& cursor);
    using EffectsHandler::createInputWindow;
    virtual void destroyInputWindow(Window w);
    virtual bool checkInputWindowEvent(XEvent* e);
    virtual void checkInputWindowStacking();

    virtual void checkElectricBorder(const QPoint &pos, Time time);
    virtual void reserveElectricBorder(ElectricBorder border);
    virtual void unreserveElectricBorder(ElectricBorder border);
    virtual void reserveElectricBorderSwitching(bool reserve, Qt::Orientations o);

    virtual unsigned long xrenderBufferPicture();
    virtual void reconfigure();
    virtual void registerPropertyType(long atom, bool reg);
    virtual QByteArray readRootProperty(long atom, long type, int format) const;
    virtual void deleteRootProperty(long atom) const;

    virtual bool hasDecorationShadows() const;

    virtual bool decorationsHaveAlpha() const;

    virtual bool decorationSupportsBlurBehind() const;

    virtual EffectFrame* effectFrame(EffectFrameStyle style, bool staticSize, const QPoint& position, Qt::Alignment alignment) const;

    virtual QVariant kwinOption(KWinOption kwopt);

    // internal (used by kwin core or compositing code)
    void startPaint();
    bool borderActivated(ElectricBorder border);
    void grabbedKeyboardEvent(QKeyEvent* e);
    bool hasKeyboardGrab() const;
    void desktopResized(const QSize &size);

    virtual void reloadEffect(Effect *effect);
    bool loadEffect(const QString& name, bool checkDefault = false);
    void toggleEffect(const QString& name);
    void unloadEffect(const QString& name);
    void reconfigureEffect(const QString& name);
    bool isEffectLoaded(const QString& name) const;
    QString supportInformation(const QString& name) const;
    QStringList loadedEffects() const;
    QStringList listOfEffects() const;

    QList<EffectWindow*> elevatedWindows() const;
    QStringList activeEffects() const;

public Q_SLOTS:
    void slotCurrentTabAboutToChange(EffectWindow* from, EffectWindow* to);
    void slotTabAdded(EffectWindow* from, EffectWindow* to);
    void slotTabRemoved(EffectWindow* c, EffectWindow* newActiveWindow);
    void slotShowOutline(const QRect &geometry);
    void slotHideOutline();

protected Q_SLOTS:
    void slotDesktopChanged(int old, KWin::Client *withClient);
    void slotClientAdded(KWin::Client *c);
    void slotClientShown(KWin::Toplevel*);
    void slotUnmanagedAdded(KWin::Unmanaged *u);
    void slotWindowClosed(KWin::Toplevel *c);
    void slotClientActivated(KWin::Client *c);
    void slotDeletedRemoved(KWin::Deleted *d);
    void slotClientMaximized(KWin::Client *c, KDecorationDefines::MaximizeMode maxMode);
    void slotClientStartUserMovedResized(KWin::Client *c);
    void slotClientStepUserMovedResized(KWin::Client *c, const QRect &geometry);
    void slotClientFinishUserMovedResized(KWin::Client *c);
    void slotOpacityChanged(KWin::Toplevel *t, qreal oldOpacity);
    void slotClientMinimized(KWin::Client *c, bool animate);
    void slotClientUnminimized(KWin::Client *c, bool animate);
    void slotGeometryShapeChanged(KWin::Toplevel *t, const QRect &old);
    void slotPaddingChanged(KWin::Toplevel *t, const QRect &old);
    void slotWindowDamaged(KWin::Toplevel *t, const QRect& r);
    void slotPropertyNotify(KWin::Toplevel *t, long atom);
    void slotPropertyNotify(long atom);

protected:
    bool loadScriptedEffect(const QString &name, KService *service);
    KLibrary* findEffectLibrary(KService* service);
    void effectsChanged();
    void setupClientConnections(KWin::Client *c);
    void setupUnmanagedConnections(KWin::Unmanaged *u);

    Effect* keyboard_grab_effect;
    Effect* fullscreen_effect;
    QList<EffectWindow*> elevated_windows;
    QMultiMap< int, EffectPair > effect_order;
    QHash< long, int > registered_atoms;
    int next_window_quad_type;
    int mouse_poll_ref_count;

private Q_SLOTS:
    void slotEffectsQueried();

private:
    QList< Effect* > m_activeEffects;
    QList< Effect* >::iterator m_currentDrawWindowIterator;
    QList< Effect* >::iterator m_currentPaintWindowIterator;
    QList< Effect* >::iterator m_currentPaintEffectFrameIterator;
    QList< Effect* >::iterator m_currentPaintScreenIterator;
    QList< Effect* >::iterator m_currentBuildQuadsIterator;
};

class EffectWindowImpl : public EffectWindow
{
    Q_OBJECT
public:
    EffectWindowImpl(Toplevel *toplevel);
    virtual ~EffectWindowImpl();

    virtual void enablePainting(int reason);
    virtual void disablePainting(int reason);
    virtual bool isPaintingEnabled();

    virtual void refWindow();
    virtual void unrefWindow();

    virtual const EffectWindowGroup* group() const;

    virtual QRegion shape() const;
    virtual QRect decorationInnerRect() const;
    virtual QByteArray readProperty(long atom, long type, int format) const;
    virtual void deleteProperty(long atom) const;

    virtual EffectWindow* findModal();
    virtual EffectWindowList mainWindows() const;

    virtual WindowQuadList buildQuads(bool force = false) const;

    const Toplevel* window() const;
    Toplevel* window();

    void setWindow(Toplevel* w);   // internal
    void setSceneWindow(Scene::Window* w);   // internal
    const Scene::Window* sceneWindow() const; // internal
    Scene::Window* sceneWindow(); // internal

    void setData(int role, const QVariant &data);
    QVariant data(int role) const;

    void registerThumbnail(ThumbnailItem *item);
    QHash<ThumbnailItem*, QWeakPointer<EffectWindowImpl> > const &thumbnails() const {
        return m_thumbnails;
    }
private Q_SLOTS:
    void thumbnailDestroyed(QObject *object);
    void thumbnailTargetChanged();
private:
    void insertThumbnail(ThumbnailItem *item);
    Toplevel* toplevel;
    Scene::Window* sw; // This one is used only during paint pass.
    QHash<int, QVariant> dataMap;
    QHash<ThumbnailItem*, QWeakPointer<EffectWindowImpl> > m_thumbnails;
};

class EffectWindowGroupImpl
    : public EffectWindowGroup
{
public:
    EffectWindowGroupImpl(Group* g);
    virtual EffectWindowList members() const;
private:
    Group* group;
};

class EffectFrameImpl
    : public QObject, public EffectFrame
{
    Q_OBJECT
public:
    explicit EffectFrameImpl(EffectFrameStyle style, bool staticSize = true, QPoint position = QPoint(-1, -1),
                             Qt::Alignment alignment = Qt::AlignCenter);
    virtual ~EffectFrameImpl();

    virtual void free();
    virtual void render(QRegion region = infiniteRegion(), double opacity = 1.0, double frameOpacity = 1.0);
    virtual Qt::Alignment alignment() const;
    virtual void setAlignment(Qt::Alignment alignment);
    virtual const QFont& font() const;
    virtual void setFont(const QFont& font);
    virtual const QRect& geometry() const;
    virtual void setGeometry(const QRect& geometry, bool force = false);
    virtual const QPixmap& icon() const;
    virtual void setIcon(const QPixmap& icon);
    virtual const QSize& iconSize() const;
    virtual void setIconSize(const QSize& size);
    virtual void setPosition(const QPoint& point);
    virtual const QString& text() const;
    virtual void setText(const QString& text);
    virtual EffectFrameStyle style() const {
        return m_style;
    };
    Plasma::FrameSvg& frame() {
        return m_frame;
    }
    bool isStatic() const {
        return m_static;
    };
    void finalRender(QRegion region, double opacity, double frameOpacity) const;
    virtual void setShader(GLShader* shader) {
        m_shader = shader;
    }
    virtual GLShader* shader() const {
        return m_shader;
    }
    virtual void setSelection(const QRect& selection);
    const QRect& selection() const {
        return m_selectionGeometry;
    }
    Plasma::FrameSvg& selectionFrame() {
        return m_selection;
    }
    /**
     * The foreground text color as specified by the default Plasma theme.
     */
    static QColor styledTextColor();

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
    QPixmap m_icon;
    QSize m_iconSize;
    QRect m_selectionGeometry;

    Scene::EffectFrame* m_sceneFrame;
    GLShader* m_shader;
};

inline
QList<EffectWindow*> EffectsHandlerImpl::elevatedWindows() const
{
    return elevated_windows;
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
