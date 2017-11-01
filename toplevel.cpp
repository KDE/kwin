/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "toplevel.h"

#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "atoms.h"
#include "client.h"
#include "client_machine.h"
#include "effects.h"
#include "screens.h"
#include "shadow.h"
#include "xcbutils.h"

#include <KWayland/Server/surface_interface.h>

#include <QDebug>

namespace KWin
{

Toplevel::Toplevel()
    : m_visual(XCB_NONE)
    , bit_depth(24)
    , info(NULL)
    , ready_for_painting(true)
    , m_isDamaged(false)
    , m_client()
    , damage_handle(None)
    , is_shape(false)
    , effect_window(NULL)
    , m_clientMachine(new ClientMachine(this))
    , wmClientLeaderWin(0)
    , m_damageReplyPending(false)
    , m_screen(0)
    , m_skipCloseAnimation(false)
{
    connect(this, SIGNAL(damaged(KWin::Toplevel*,QRect)), SIGNAL(needsRepaint()));
    connect(screens(), SIGNAL(changed()), SLOT(checkScreen()));
    connect(screens(), SIGNAL(countChanged(int,int)), SLOT(checkScreen()));
    setupCheckScreenConnection();
}

Toplevel::~Toplevel()
{
    assert(damage_handle == None);
    delete info;
}

QDebug& operator<<(QDebug& stream, const Toplevel* cl)
{
    if (cl == NULL)
        return stream << "\'NULL\'";
    cl->debug(stream);
    return stream;
}

QDebug& operator<<(QDebug& stream, const ToplevelList& list)
{
    stream << "LIST:(";
    bool first = true;
    for (ToplevelList::ConstIterator it = list.begin();
            it != list.end();
            ++it) {
        if (!first)
            stream << ":";
        first = false;
        stream << *it;
    }
    stream << ")";
    return stream;
}

QRect Toplevel::decorationRect() const
{
    return rect();
}

void Toplevel::detectShape(Window id)
{
    const bool wasShape = is_shape;
    is_shape = Xcb::Extensions::self()->hasShape(id);
    if (wasShape != is_shape) {
        emit shapedChanged();
    }
}

// used only by Deleted::copy()
void Toplevel::copyToDeleted(Toplevel* c)
{
    geom = c->geom;
    m_visual = c->m_visual;
    bit_depth = c->bit_depth;
    info = c->info;
    m_client.reset(c->m_client, false);
    ready_for_painting = c->ready_for_painting;
    damage_handle = None;
    damage_region = c->damage_region;
    repaints_region = c->repaints_region;
    is_shape = c->is_shape;
    effect_window = c->effect_window;
    if (effect_window != NULL)
        effect_window->setWindow(this);
    resource_name = c->resourceName();
    resource_class = c->resourceClass();
    m_clientMachine = c->m_clientMachine;
    m_clientMachine->setParent(this);
    wmClientLeaderWin = c->wmClientLeader();
    opaque_region = c->opaqueRegion();
    m_screen = c->m_screen;
    m_skipCloseAnimation = c->m_skipCloseAnimation;
    m_internalFBO = c->m_internalFBO;
}

// before being deleted, remove references to everything that's now
// owner by Deleted
void Toplevel::disownDataPassedToDeleted()
{
    info = NULL;
}

QRect Toplevel::visibleRect() const
{
    QRect r = decorationRect();
    if (hasShadow() && !shadow()->shadowRegion().isEmpty()) {
        r |= shadow()->shadowRegion().boundingRect();
    }
    return r.translated(geometry().topLeft());
}

Xcb::Property Toplevel::fetchWmClientLeader() const
{
    return Xcb::Property(false, window(), atoms->wm_client_leader, XCB_ATOM_WINDOW, 0, 10000);
}

void Toplevel::readWmClientLeader(Xcb::Property &prop)
{
    wmClientLeaderWin = prop.value<xcb_window_t>(window());
}

void Toplevel::getWmClientLeader()
{
    auto prop = fetchWmClientLeader();
    readWmClientLeader(prop);
}

/*!
  Returns sessionId for this client,
  taken either from its window or from the leader window.
 */
QByteArray Toplevel::sessionId() const
{
    QByteArray result = Xcb::StringProperty(window(), atoms->sm_client_id);
    if (result.isEmpty() && wmClientLeaderWin && wmClientLeaderWin != window())
        result = Xcb::StringProperty(wmClientLeaderWin, atoms->sm_client_id);
    return result;
}

/*!
  Returns command property for this client,
  taken either from its window or from the leader window.
 */
QByteArray Toplevel::wmCommand()
{
    QByteArray result = Xcb::StringProperty(window(), XCB_ATOM_WM_COMMAND);
    if (result.isEmpty() && wmClientLeaderWin && wmClientLeaderWin != window())
        result = Xcb::StringProperty(wmClientLeaderWin, XCB_ATOM_WM_COMMAND);
    result.replace(0, ' ');
    return result;
}

void Toplevel::getWmClientMachine()
{
    m_clientMachine->resolve(window(), wmClientLeader());
}

/*!
  Returns client machine for this client,
  taken either from its window or from the leader window.
*/
QByteArray Toplevel::wmClientMachine(bool use_localhost) const
{
    if (!m_clientMachine) {
        // this should never happen
        return QByteArray();
    }
    if (use_localhost && m_clientMachine->isLocal()) {
        // special name for the local machine (localhost)
        return ClientMachine::localhost();
    }
    return m_clientMachine->hostName();
}

/*!
  Returns client leader window for this client.
  Returns the client window itself if no leader window is defined.
*/
Window Toplevel::wmClientLeader() const
{
    if (wmClientLeaderWin)
        return wmClientLeaderWin;
    return window();
}

void Toplevel::getResourceClass()
{
    setResourceClass(QByteArray(info->windowClassName()).toLower(), QByteArray(info->windowClassClass()).toLower());
}

void Toplevel::setResourceClass(const QByteArray &name, const QByteArray &className)
{
    resource_name  = name;
    resource_class = className;
    emit windowClassChanged();
}

double Toplevel::opacity() const
{
    if (info->opacity() == 0xffffffff)
        return 1.0;
    return info->opacity() * 1.0 / 0xffffffff;
}

void Toplevel::setOpacity(double new_opacity)
{
    double old_opacity = opacity();
    new_opacity = qBound(0.0, new_opacity, 1.0);
    if (old_opacity == new_opacity)
        return;
    info->setOpacity(static_cast< unsigned long >(new_opacity * 0xffffffff));
    if (compositing()) {
        addRepaintFull();
        emit opacityChanged(this, old_opacity);
    }
}

void Toplevel::setReadyForPainting()
{
    if (!ready_for_painting) {
        ready_for_painting = true;
        if (compositing()) {
            addRepaintFull();
            emit windowShown(this);
            if (Client *cl = dynamic_cast<Client*>(this)) {
                if (cl->tabGroup() && cl->tabGroup()->current() == cl)
                    cl->tabGroup()->setCurrent(cl, true);
            }
        }
    }
}

void Toplevel::deleteEffectWindow()
{
    delete effect_window;
    effect_window = NULL;
}

void Toplevel::checkScreen()
{
    if (screens()->count() == 1) {
        if (m_screen != 0) {
            m_screen = 0;
            emit screenChanged();
        }
    } else {
        const int s = screens()->number(geometry().center());
        if (s != m_screen) {
            m_screen = s;
            emit screenChanged();
        }
    }
    qreal newScale = screens()->scale(m_screen);
    if (newScale != m_screenScale) {
        m_screenScale = newScale;
        emit screenScaleChanged();
    }
}

void Toplevel::setupCheckScreenConnection()
{
    connect(this, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), SLOT(checkScreen()));
    connect(this, SIGNAL(geometryChanged()), SLOT(checkScreen()));
    checkScreen();
}

void Toplevel::removeCheckScreenConnection()
{
    disconnect(this, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), this, SLOT(checkScreen()));
    disconnect(this, SIGNAL(geometryChanged()), this, SLOT(checkScreen()));
}

int Toplevel::screen() const
{
    return m_screen;
}

qreal Toplevel::screenScale() const
{
    return m_screenScale;
}

bool Toplevel::isOnScreen(int screen) const
{
    return screens()->geometry(screen).intersects(geometry());
}

bool Toplevel::isOnActiveScreen() const
{
    return isOnScreen(screens()->current());
}

void Toplevel::getShadow()
{
    QRect dirtyRect;  // old & new shadow region
    const QRect oldVisibleRect = visibleRect();
    if (hasShadow()) {
        dirtyRect = shadow()->shadowRegion().boundingRect();
        effectWindow()->sceneWindow()->shadow()->updateShadow();
    } else {
        Shadow::createShadow(this);
    }
    if (hasShadow())
        dirtyRect |= shadow()->shadowRegion().boundingRect();
    if (oldVisibleRect != visibleRect())
        emit paddingChanged(this, oldVisibleRect);
    if (dirtyRect.isValid()) {
        dirtyRect.translate(pos());
        addLayerRepaint(dirtyRect);
    }
}

bool Toplevel::hasShadow() const
{
    if (effectWindow() && effectWindow()->sceneWindow()) {
        return effectWindow()->sceneWindow()->shadow() != NULL;
    }
    return false;
}

Shadow *Toplevel::shadow()
{
    if (effectWindow() && effectWindow()->sceneWindow()) {
        return effectWindow()->sceneWindow()->shadow();
    } else {
        return NULL;
    }
}

const Shadow *Toplevel::shadow() const
{
    if (effectWindow() && effectWindow()->sceneWindow()) {
        return effectWindow()->sceneWindow()->shadow();
    } else {
        return NULL;
    }
}

bool Toplevel::wantsShadowToBeRendered() const
{
    return true;
}

void Toplevel::getWmOpaqueRegion()
{
    const auto rects = info->opaqueRegion();
    QRegion new_opaque_region;
    for (const auto &r : rects) {
        new_opaque_region += QRect(r.pos.x, r.pos.y, r.size.width, r.size.height);
    }

    opaque_region = new_opaque_region;
}

bool Toplevel::isClient() const
{
    return false;
}

bool Toplevel::isDeleted() const
{
    return false;
}

bool Toplevel::isOnCurrentActivity() const
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return true;
    }
    return isOnActivity(Activities::self()->current());
#else
    return true;
#endif
}

void Toplevel::elevate(bool elevate)
{
    if (!effectWindow()) {
        return;
    }
    effectWindow()->elevate(elevate);
    addWorkspaceRepaint(visibleRect());
}

pid_t Toplevel::pid() const
{
    return info->pid();
}

xcb_window_t Toplevel::frameId() const
{
    return m_client;
}

Xcb::Property Toplevel::fetchSkipCloseAnimation() const
{
    return Xcb::Property(false, window(), atoms->kde_skip_close_animation, XCB_ATOM_CARDINAL, 0, 1);
}

void Toplevel::readSkipCloseAnimation(Xcb::Property &property)
{
    setSkipCloseAnimation(property.toBool());
}

void Toplevel::getSkipCloseAnimation()
{
    Xcb::Property property = fetchSkipCloseAnimation();
    readSkipCloseAnimation(property);
}

bool Toplevel::skipsCloseAnimation() const
{
    return m_skipCloseAnimation;
}

void Toplevel::setSkipCloseAnimation(bool set)
{
    if (set == m_skipCloseAnimation) {
        return;
    }
    m_skipCloseAnimation = set;
    emit skipCloseAnimationChanged();
}

void Toplevel::setSurface(KWayland::Server::SurfaceInterface *surface)
{
    if (m_surface == surface) {
        return;
    }
    using namespace KWayland::Server;
    if (m_surface) {
        disconnect(m_surface, &SurfaceInterface::damaged, this, &Toplevel::addDamage);
        disconnect(m_surface, &SurfaceInterface::sizeChanged, this, &Toplevel::discardWindowPixmap);
    }
    m_surface = surface;
    connect(m_surface, &SurfaceInterface::damaged, this, &Toplevel::addDamage);
    connect(m_surface, &SurfaceInterface::sizeChanged, this, &Toplevel::discardWindowPixmap);
    connect(m_surface, &SurfaceInterface::subSurfaceTreeChanged, this,
        [this] {
            // TODO improve to only update actual visual area
            if (ready_for_painting) {
                addDamageFull();
                m_isDamaged = true;
            }
        }
    );
    connect(m_surface, &SurfaceInterface::destroyed, this,
        [this] {
            m_surface = nullptr;
        }
    );
    emit surfaceChanged();
}

void Toplevel::addDamage(const QRegion &damage)
{
    m_isDamaged = true;
    damage_region += damage;
    for (const QRect &r : damage.rects()) {
        emit damaged(this, r);
    }
}

QByteArray Toplevel::windowRole() const
{
    return QByteArray(info->windowRole());
}

void Toplevel::setDepth(int depth)
{
    if (bit_depth == depth) {
        return;
    }
    const bool oldAlpha = hasAlpha();
    bit_depth = depth;
    if (oldAlpha != hasAlpha()) {
        emit hasAlphaChanged();
    }
}

QRegion Toplevel::inputShape() const
{
    if (m_surface) {
        return m_surface->input();
    } else {
        // TODO: maybe also for X11?
        return QRegion();
    }
}

void Toplevel::setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo)
{
    if (m_internalFBO != fbo) {
        discardWindowPixmap();
        m_internalFBO = fbo;
    }
    setDepth(32);
}

QMatrix4x4 Toplevel::inputTransformation() const
{
    QMatrix4x4 m;
    m.translate(-x(), -y());
    return m;
}

quint32 Toplevel::windowId() const
{
    return window();
}

QRect Toplevel::inputGeometry() const
{
    return geometry();
}

} // namespace

