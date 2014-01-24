/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "kwineffects.h"

#include "kwinxrenderutils.h"
#include "config-kwin.h"

#include <QtDBus/QtDBus>
#include <QVariant>
#include <QList>
#include <QTimeLine>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QVector2D>
#include <QGraphicsRotation>
#include <QGraphicsScale>

#include <kdebug.h>
#include <ksharedconfig.h>
#include <kstandarddirs.h>
#include <kconfiggroup.h>

#include <assert.h>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xfixes.h>
#include <xcb/xfixes.h>
#endif

#if defined(__GNUC__)
#  define KWIN_ALIGN(n) __attribute((aligned(n)))
#  if defined(__SSE2__)
#    define HAVE_SSE2
#  endif
#elif defined(__INTEL_COMPILER)
#  define KWIN_ALIGN(n) __declspec(align(n))
#  define HAVE_SSE2
#else
#  define KWIN_ALIGN(n)
#endif

#ifdef HAVE_SSE2
#  include <emmintrin.h>
#endif


namespace KWin
{

void WindowPrePaintData::setTranslucent()
{
    mask |= Effect::PAINT_WINDOW_TRANSLUCENT;
    mask &= ~Effect::PAINT_WINDOW_OPAQUE;
    clip = QRegion(); // cannot clip, will be transparent
}

void WindowPrePaintData::setTransformed()
{
    mask |= Effect::PAINT_WINDOW_TRANSFORMED;
}

class PaintDataPrivate {
public:
    QGraphicsScale scale;
    QVector3D translation;
    QGraphicsRotation rotation;
};

PaintData::PaintData()
    : d(new PaintDataPrivate())
{
}

PaintData::~PaintData()
{
    delete d;
}

qreal PaintData::xScale() const
{
    return d->scale.xScale();
}

qreal PaintData::yScale() const
{
    return d->scale.yScale();
}

qreal PaintData::zScale() const
{
    return d->scale.zScale();
}

void PaintData::setScale(const QVector2D &scale)
{
    d->scale.setXScale(scale.x());
    d->scale.setYScale(scale.y());
}

void PaintData::setScale(const QVector3D &scale)
{
    d->scale.setXScale(scale.x());
    d->scale.setYScale(scale.y());
    d->scale.setZScale(scale.z());
}

void PaintData::setXScale(qreal scale)
{
    d->scale.setXScale(scale);
}

void PaintData::setYScale(qreal scale)
{
    d->scale.setYScale(scale);
}

void PaintData::setZScale(qreal scale)
{
    d->scale.setZScale(scale);
}

const QGraphicsScale &PaintData::scale() const
{
    return d->scale;
}

void PaintData::setXTranslation(qreal translate)
{
    d->translation.setX(translate);
}

void PaintData::setYTranslation(qreal translate)
{
    d->translation.setY(translate);
}

void PaintData::setZTranslation(qreal translate)
{
    d->translation.setZ(translate);
}

void PaintData::translate(qreal x, qreal y, qreal z)
{
    translate(QVector3D(x, y, z));
}

void PaintData::translate(const QVector3D &t)
{
    d->translation += t;
}

qreal PaintData::xTranslation() const
{
    return d->translation.x();
}

qreal PaintData::yTranslation() const
{
    return d->translation.y();
}

qreal PaintData::zTranslation() const
{
    return d->translation.z();
}

const QVector3D &PaintData::translation() const
{
    return d->translation;
}

qreal PaintData::rotationAngle() const
{
    return d->rotation.angle();
}

QVector3D PaintData::rotationAxis() const
{
    return d->rotation.axis();
}

QVector3D PaintData::rotationOrigin() const
{
    return d->rotation.origin();
}

void PaintData::setRotationAngle(qreal angle)
{
    d->rotation.setAngle(angle);
}

void PaintData::setRotationAxis(Qt::Axis axis)
{
    d->rotation.setAxis(axis);
}

void PaintData::setRotationAxis(const QVector3D &axis)
{
    d->rotation.setAxis(axis);
}

void PaintData::setRotationOrigin(const QVector3D &origin)
{
    d->rotation.setOrigin(origin);
}

class WindowPaintDataPrivate {
public:
    qreal opacity;
    qreal decorationOpacity;
    qreal saturation;
    qreal brightness;
    int screen;
    qreal crossFadeProgress;
};

WindowPaintData::WindowPaintData(EffectWindow* w)
    : PaintData()
    , shader(NULL)
    , d(new WindowPaintDataPrivate())
{
    quads = w->buildQuads();
    setOpacity(w->opacity());
    setDecorationOpacity(1.0);
    setSaturation(1.0);
    setBrightness(1.0);
    setScreen(0);
    setCrossFadeProgress(1.0);
}

WindowPaintData::WindowPaintData(const WindowPaintData &other)
    : PaintData()
    , quads(other.quads)
    , shader(other.shader)
    , d(new WindowPaintDataPrivate())
{
    setXScale(other.xScale());
    setYScale(other.yScale());
    setZScale(other.zScale());
    translate(other.translation());
    setRotationOrigin(other.rotationOrigin());
    setRotationAxis(other.rotationAxis());
    setRotationAngle(other.rotationAngle());
    setOpacity(other.opacity());
    setDecorationOpacity(other.decorationOpacity());
    setSaturation(other.saturation());
    setBrightness(other.brightness());
    setScreen(other.screen());
    setCrossFadeProgress(other.crossFadeProgress());
}

WindowPaintData::~WindowPaintData()
{
    delete d;
}

qreal WindowPaintData::decorationOpacity() const
{
    return d->decorationOpacity;
}

qreal WindowPaintData::opacity() const
{
    return d->opacity;
}

qreal WindowPaintData::saturation() const
{
    return d->saturation;
}

qreal WindowPaintData::brightness() const
{
    return d->brightness;
}

int WindowPaintData::screen() const
{
    return d->screen;
}

void WindowPaintData::setDecorationOpacity(qreal opacity)
{
    d->decorationOpacity = opacity;
}

void WindowPaintData::setOpacity(qreal opacity)
{
    d->opacity = opacity;
}

void WindowPaintData::setSaturation(qreal saturation) const
{
    d->saturation = saturation;
}

void WindowPaintData::setBrightness(qreal brightness)
{
    d->brightness = brightness;
}

void WindowPaintData::setScreen(int screen) const
{
    d->screen = screen;
}

qreal WindowPaintData::crossFadeProgress() const
{
    return d->crossFadeProgress;
}

void WindowPaintData::setCrossFadeProgress(qreal factor)
{
    d->crossFadeProgress = qBound(qreal(0.0), factor, qreal(1.0));
}

qreal WindowPaintData::multiplyDecorationOpacity(qreal factor)
{
    d->decorationOpacity *= factor;
    return d->decorationOpacity;
}

qreal WindowPaintData::multiplyOpacity(qreal factor)
{
    d->opacity *= factor;
    return d->opacity;
}

qreal WindowPaintData::multiplySaturation(qreal factor)
{
    d->saturation *= factor;
    return d->saturation;
}

qreal WindowPaintData::multiplyBrightness(qreal factor)
{
    d->brightness *= factor;
    return d->brightness;
}

WindowPaintData &WindowPaintData::operator*=(qreal scale)
{
    this->setXScale(this->xScale() * scale);
    this->setYScale(this->yScale() * scale);
    this->setZScale(this->zScale() * scale);
    return *this;
}

WindowPaintData &WindowPaintData::operator*=(const QVector2D &scale)
{
    this->setXScale(this->xScale() * scale.x());
    this->setYScale(this->yScale() * scale.y());
    return *this;
}

WindowPaintData &WindowPaintData::operator*=(const QVector3D &scale)
{
    this->setXScale(this->xScale() * scale.x());
    this->setYScale(this->yScale() * scale.y());
    this->setZScale(this->zScale() * scale.z());
    return *this;
}

WindowPaintData &WindowPaintData::operator+=(const QPointF &translation)
{
    return this->operator+=(QVector3D(translation));
}

WindowPaintData &WindowPaintData::operator+=(const QPoint &translation)
{
    return this->operator+=(QVector3D(translation));
}

WindowPaintData &WindowPaintData::operator+=(const QVector2D &translation)
{
    return this->operator+=(QVector3D(translation));
}

WindowPaintData &WindowPaintData::operator+=(const QVector3D &translation)
{
    translate(translation);
    return *this;
}

ScreenPaintData::ScreenPaintData()
    : PaintData()
{
}

ScreenPaintData::ScreenPaintData(const ScreenPaintData &other)
    : PaintData()
{
    translate(other.translation());
    setXScale(other.xScale());
    setYScale(other.yScale());
    setZScale(other.zScale());
    setRotationOrigin(other.rotationOrigin());
    setRotationAxis(other.rotationAxis());
    setRotationAngle(other.rotationAngle());
}

ScreenPaintData &ScreenPaintData::operator=(const ScreenPaintData &rhs)
{
    setXScale(rhs.xScale());
    setYScale(rhs.yScale());
    setZScale(rhs.zScale());
    setXTranslation(rhs.xTranslation());
    setYTranslation(rhs.yTranslation());
    setZTranslation(rhs.zTranslation());
    setRotationOrigin(rhs.rotationOrigin());
    setRotationAxis(rhs.rotationAxis());
    setRotationAngle(rhs.rotationAngle());
    return *this;
}

ScreenPaintData &ScreenPaintData::operator*=(qreal scale)
{
    setXScale(this->xScale() * scale);
    setYScale(this->yScale() * scale);
    setZScale(this->zScale() * scale);
    return *this;
}

ScreenPaintData &ScreenPaintData::operator*=(const QVector2D &scale)
{
    setXScale(this->xScale() * scale.x());
    setYScale(this->yScale() * scale.y());
    return *this;
}

ScreenPaintData &ScreenPaintData::operator*=(const QVector3D &scale)
{
    setXScale(this->xScale() * scale.x());
    setYScale(this->yScale() * scale.y());
    setZScale(this->zScale() * scale.z());
    return *this;
}

ScreenPaintData &ScreenPaintData::operator+=(const QPointF &translation)
{
    return this->operator+=(QVector3D(translation));
}

ScreenPaintData &ScreenPaintData::operator+=(const QPoint &translation)
{
    return this->operator+=(QVector3D(translation));
}

ScreenPaintData &ScreenPaintData::operator+=(const QVector2D &translation)
{
    return this->operator+=(QVector3D(translation));
}

ScreenPaintData &ScreenPaintData::operator+=(const QVector3D &translation)
{
    translate(translation);
    return *this;
}

//****************************************
// Effect
//****************************************

Effect::Effect()
{
}

Effect::~Effect()
{
}

void Effect::reconfigure(ReconfigureFlags)
{
}

void* Effect::proxy()
{
    return NULL;
}

void Effect::windowInputMouseEvent(QEvent*)
{
}

void Effect::grabbedKeyboardEvent(QKeyEvent*)
{
}

bool Effect::borderActivated(ElectricBorder)
{
    return false;
}

void Effect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    effects->prePaintScreen(data, time);
}

void Effect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);
}

void Effect::postPaintScreen()
{
    effects->postPaintScreen();
}

void Effect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    effects->prePaintWindow(w, data, time);
}

void Effect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    effects->paintWindow(w, mask, region, data);
}

void Effect::postPaintWindow(EffectWindow* w)
{
    effects->postPaintWindow(w);
}

void Effect::paintEffectFrame(KWin::EffectFrame* frame, QRegion region, double opacity, double frameOpacity)
{
    effects->paintEffectFrame(frame, region, opacity, frameOpacity);
}

bool Effect::provides(Feature)
{
    return false;
}

bool Effect::isActive() const
{
    return true;
}

QString Effect::debug(const QString &) const
{
    return QString();
}

void Effect::drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    effects->drawWindow(w, mask, region, data);
}

void Effect::buildQuads(EffectWindow* w, WindowQuadList& quadList)
{
    effects->buildQuads(w, quadList);
}

void Effect::setPositionTransformations(WindowPaintData& data, QRect& region, EffectWindow* w,
                                        const QRect& r, Qt::AspectRatioMode aspect)
{
    QSize size = w->size();
    size.scale(r.size(), aspect);
    data.setXScale(size.width() / double(w->width()));
    data.setYScale(size.height() / double(w->height()));
    int width = int(w->width() * data.xScale());
    int height = int(w->height() * data.yScale());
    int x = r.x() + (r.width() - width) / 2;
    int y = r.y() + (r.height() - height) / 2;
    region = QRect(x, y, width, height);
    data.setXTranslation(x - w->x());
    data.setYTranslation(y - w->y());
}

int Effect::displayWidth()
{
    return KWin::displayWidth();
}

int Effect::displayHeight()
{
    return KWin::displayHeight();
}

QPoint Effect::cursorPos()
{
    return effects->cursorPos();
}

double Effect::animationTime(const KConfigGroup& cfg, const QString& key, int defaultTime)
{
    int time = cfg.readEntry(key, 0);
    return time != 0 ? time : qMax(defaultTime * effects->animationTimeFactor(), 1.);
}

double Effect::animationTime(int defaultTime)
{
    // at least 1ms, otherwise 0ms times can break some things
    return qMax(defaultTime * effects->animationTimeFactor(), 1.);
}

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler(CompositingType type)
    : compositing_type(type)
{
    if (compositing_type == NoCompositing)
        return;
    KWin::effects = this;
}

EffectsHandler::~EffectsHandler()
{
    // All effects should already be unloaded by Impl dtor
    assert(loaded_effects.count() == 0);
}

CompositingType EffectsHandler::compositingType() const
{
    return compositing_type;
}

bool EffectsHandler::isOpenGLCompositing() const
{
    return compositing_type & OpenGLCompositing;
}

void EffectsHandler::sendReloadMessage(const QString& effectname)
{
    QDBusMessage message = QDBusMessage::createMethodCall("org.kde.kwin", "/KWin", "org.kde.KWin", "reconfigureEffect");
    message << QString("kwin4_effect_" + effectname);
    QDBusConnection::sessionBus().send(message);
}

KConfigGroup EffectsHandler::effectConfig(const QString& effectname)
{
    KSharedConfig::Ptr kwinconfig = KSharedConfig::openConfig(KWIN_CONFIG, KConfig::NoGlobals);
    return kwinconfig->group("Effect-" + effectname);
}

EffectsHandler* effects = 0;


//****************************************
// EffectWindow
//****************************************

EffectWindow::EffectWindow(QObject *parent)
    : QObject(parent)
{
}

EffectWindow::~EffectWindow()
{
}

#define WINDOW_HELPER( rettype, prototype, propertyname ) \
    rettype EffectWindow::prototype ( ) const \
    { \
        return parent()->property( propertyname ).value< rettype >(); \
    }

WINDOW_HELPER(double, opacity, "opacity")
WINDOW_HELPER(bool, hasAlpha, "alpha")
WINDOW_HELPER(int, x, "x")
WINDOW_HELPER(int, y, "y")
WINDOW_HELPER(int, width, "width")
WINDOW_HELPER(int, height, "height")
WINDOW_HELPER(QPoint, pos, "pos")
WINDOW_HELPER(QSize, size, "size")
WINDOW_HELPER(int, screen, "screen")
WINDOW_HELPER(QRect, geometry, "geometry")
WINDOW_HELPER(QRect, expandedGeometry, "visibleRect")
WINDOW_HELPER(QRect, rect, "rect")
WINDOW_HELPER(int, desktop, "desktop")
WINDOW_HELPER(bool, isDesktop, "desktopWindow")
WINDOW_HELPER(bool, isDock, "dock")
WINDOW_HELPER(bool, isToolbar, "toolbar")
WINDOW_HELPER(bool, isMenu, "menu")
WINDOW_HELPER(bool, isNormalWindow, "normalWindow")
WINDOW_HELPER(bool, isDialog, "dialog")
WINDOW_HELPER(bool, isSplash, "splash")
WINDOW_HELPER(bool, isUtility, "utility")
WINDOW_HELPER(bool, isDropdownMenu, "dropdownMenu")
WINDOW_HELPER(bool, isPopupMenu, "popupMenu")
WINDOW_HELPER(bool, isTooltip, "tooltip")
WINDOW_HELPER(bool, isNotification, "notification")
WINDOW_HELPER(bool, isComboBox, "comboBox")
WINDOW_HELPER(bool, isDNDIcon, "dndIcon")
WINDOW_HELPER(bool, isManaged, "managed")
WINDOW_HELPER(bool, isDeleted, "deleted")
WINDOW_HELPER(bool, hasOwnShape, "shaped")
WINDOW_HELPER(QString, windowRole, "windowRole")
WINDOW_HELPER(QStringList, activities, "activities")
WINDOW_HELPER(bool, skipsCloseAnimation, "skipsCloseAnimation")

QString EffectWindow::windowClass() const
{
    return parent()->property("resourceName").toString() + ' ' + parent()->property("resourceClass").toString();
}

QRect EffectWindow::contentsRect() const
{
    return QRect(parent()->property("clientPos").toPoint(), parent()->property("clientSize").toSize());
}

NET::WindowType EffectWindow::windowType() const
{
    return static_cast<NET::WindowType>(parent()->property("windowType").toInt());
}

bool EffectWindow::isOnActivity(QString activity) const
{
    const QStringList activities = parent()->property("activities").toStringList();
    return activities.isEmpty() || activities.contains(activity);
}

bool EffectWindow::isOnAllActivities() const
{
    return parent()->property("activities").toStringList().isEmpty();
}

#undef WINDOW_HELPER

#define WINDOW_HELPER_DEFAULT( rettype, prototype, propertyname, defaultValue ) \
    rettype EffectWindow::prototype ( ) const \
    { \
        const QVariant variant = parent()->property( propertyname ); \
        if (!variant.isValid()) { \
            return defaultValue; \
        } \
        return variant.value< rettype >(); \
    }

WINDOW_HELPER_DEFAULT(bool, isMinimized, "minimized", false)
WINDOW_HELPER_DEFAULT(bool, isMovable, "moveable", false)
WINDOW_HELPER_DEFAULT(bool, isMovableAcrossScreens, "moveableAcrossScreens", false)
WINDOW_HELPER_DEFAULT(QString, caption, "caption", "")
WINDOW_HELPER_DEFAULT(bool, keepAbove, "keepAbove", true)
WINDOW_HELPER_DEFAULT(bool, isModal, "modal", false)
WINDOW_HELPER_DEFAULT(QSize, basicUnit, "basicUnit", QSize(1, 1))
WINDOW_HELPER_DEFAULT(bool, isUserMove, "move", false)
WINDOW_HELPER_DEFAULT(bool, isUserResize, "resize", false)
WINDOW_HELPER_DEFAULT(QRect, iconGeometry, "iconGeometry", QRect())
WINDOW_HELPER_DEFAULT(bool, isSpecialWindow, "specialWindow", true)
WINDOW_HELPER_DEFAULT(bool, acceptsFocus, "wantsInput", true) // We don't actually know...
WINDOW_HELPER_DEFAULT(QPixmap, icon, "icon", QPixmap())
WINDOW_HELPER_DEFAULT(bool, isSkipSwitcher, "skipSwitcher", false)
WINDOW_HELPER_DEFAULT(bool, isCurrentTab, "isCurrentTab", false)
WINDOW_HELPER_DEFAULT(bool, decorationHasAlpha, "decorationHasAlpha", false)

#undef WINDOW_HELPER_DEFAULT

#define WINDOW_HELPER_SETTER( prototype, propertyname, args, value ) \
    void EffectWindow::prototype ( args ) \
    {\
        const QVariant variant = parent()->property( propertyname ); \
        if (variant.isValid()) { \
            parent()->setProperty( propertyname, value ); \
        } \
    }

WINDOW_HELPER_SETTER(minimize, "minimized",,true)
WINDOW_HELPER_SETTER(unminimize, "minimized",,false)

#undef WINDOW_HELPER_SETTER

void EffectWindow::setMinimized(bool min)
{
    if (min) {
        minimize();
    } else {
        unminimize();
    }
}

void EffectWindow::closeWindow() const
{
    QMetaObject::invokeMethod(parent(), "closeWindow");
}

void EffectWindow::addRepaint(int x, int y, int w, int h)
{
    QMetaObject::invokeMethod(parent(), "addRepaint", Q_ARG(int, x), Q_ARG(int, y), Q_ARG(int, w), Q_ARG(int, h));
}

void EffectWindow::addRepaint(const QRect &r)
{
    QMetaObject::invokeMethod(parent(), "addRepaint", Q_ARG(const QRect&, r));
}

void EffectWindow::addRepaintFull()
{
    QMetaObject::invokeMethod(parent(), "addRepaintFull");
}

void EffectWindow::addLayerRepaint(int x, int y, int w, int h)
{
    QMetaObject::invokeMethod(parent(), "addLayerRepaint", Q_ARG(int, x), Q_ARG(int, y), Q_ARG(int, w), Q_ARG(int, h));
}

void EffectWindow::addLayerRepaint(const QRect &r)
{
    QMetaObject::invokeMethod(parent(), "addLayerRepaint", Q_ARG(const QRect&, r));
}

bool EffectWindow::isOnCurrentActivity() const
{
    return isOnActivity(effects->currentActivity());
}

bool EffectWindow::isOnCurrentDesktop() const
{
    return isOnDesktop(effects->currentDesktop());
}

bool EffectWindow::isOnDesktop(int d) const
{
    return desktop() == d || isOnAllDesktops();
}

bool EffectWindow::isOnAllDesktops() const
{
    return desktop() == NET::OnAllDesktops;
}

bool EffectWindow::hasDecoration() const
{
    return contentsRect() != QRect(0, 0, width(), height());
}

bool EffectWindow::isVisible() const
{
    return !isMinimized()
           && isOnCurrentDesktop()
           && isOnCurrentActivity();
}


//****************************************
// EffectWindowGroup
//****************************************

EffectWindowGroup::~EffectWindowGroup()
{
}

//****************************************
// GlobalShortcutsEditor
//****************************************

GlobalShortcutsEditor::GlobalShortcutsEditor(QWidget *parent) :
    KShortcutsEditor(parent, GlobalAction)
{
}

/***************************************************************
 WindowQuad
***************************************************************/

WindowQuad WindowQuad::makeSubQuad(double x1, double y1, double x2, double y2) const
{
    assert(x1 < x2 && y1 < y2 && x1 >= left() && x2 <= right() && y1 >= top() && y2 <= bottom());
#ifndef NDEBUG
    if (isTransformed())
        kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
    WindowQuad ret(*this);
    // vertices are clockwise starting from topleft
    ret.verts[ 0 ].px = x1;
    ret.verts[ 3 ].px = x1;
    ret.verts[ 1 ].px = x2;
    ret.verts[ 2 ].px = x2;
    ret.verts[ 0 ].py = y1;
    ret.verts[ 1 ].py = y1;
    ret.verts[ 2 ].py = y2;
    ret.verts[ 3 ].py = y2;
    // original x/y are supposed to be the same, no transforming is done here
    ret.verts[ 0 ].ox = x1;
    ret.verts[ 3 ].ox = x1;
    ret.verts[ 1 ].ox = x2;
    ret.verts[ 2 ].ox = x2;
    ret.verts[ 0 ].oy = y1;
    ret.verts[ 1 ].oy = y1;
    ret.verts[ 2 ].oy = y2;
    ret.verts[ 3 ].oy = y2;
    double my_tleft = verts[ 0 ].tx;
    double my_tright = verts[ 2 ].tx;
    double my_ttop = verts[ 0 ].ty;
    double my_tbottom = verts[ 2 ].ty;
    double tleft = (x1 - left()) / (right() - left()) * (my_tright - my_tleft) + my_tleft;
    double tright = (x2 - left()) / (right() - left()) * (my_tright - my_tleft) + my_tleft;
    double ttop = (y1 - top()) / (bottom() - top()) * (my_tbottom - my_ttop) + my_ttop;
    double tbottom = (y2 - top()) / (bottom() - top()) * (my_tbottom - my_ttop) + my_ttop;
    ret.verts[ 0 ].tx = tleft;
    ret.verts[ 3 ].tx = tleft;
    ret.verts[ 1 ].tx = tright;
    ret.verts[ 2 ].tx = tright;
    ret.verts[ 0 ].ty = ttop;
    ret.verts[ 1 ].ty = ttop;
    ret.verts[ 2 ].ty = tbottom;
    ret.verts[ 3 ].ty = tbottom;
    return ret;
}

bool WindowQuad::smoothNeeded() const
{
    // smoothing is needed if the width or height of the quad does not match the original size
    double width = verts[ 1 ].ox - verts[ 0 ].ox;
    double height = verts[ 2 ].oy - verts[ 1 ].oy;
    return(verts[ 1 ].px - verts[ 0 ].px != width || verts[ 2 ].px - verts[ 3 ].px != width
           || verts[ 2 ].py - verts[ 1 ].py != height || verts[ 3 ].py - verts[ 0 ].py != height);
}

/***************************************************************
 WindowQuadList
***************************************************************/

WindowQuadList WindowQuadList::splitAtX(double x) const
{
    WindowQuadList ret;
    foreach (const WindowQuad & quad, *this) {
#ifndef NDEBUG
        if (quad.isTransformed())
            kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        bool wholeleft = true;
        bool wholeright = true;
        for (int i = 0;
                i < 4;
                ++i) {
            if (quad[ i ].x() < x)
                wholeright = false;
            if (quad[ i ].x() > x)
                wholeleft = false;
        }
        if (wholeleft || wholeright) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.left() == quad.right()) { // quad has no size
            ret.append(quad);
            continue;
        }
        ret.append(quad.makeSubQuad(quad.left(), quad.top(), x, quad.bottom()));
        ret.append(quad.makeSubQuad(x, quad.top(), quad.right(), quad.bottom()));
    }
    return ret;
}

WindowQuadList WindowQuadList::splitAtY(double y) const
{
    WindowQuadList ret;
    foreach (const WindowQuad & quad, *this) {
#ifndef NDEBUG
        if (quad.isTransformed())
            kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        bool wholetop = true;
        bool wholebottom = true;
        for (int i = 0;
                i < 4;
                ++i) {
            if (quad[ i ].y() < y)
                wholebottom = false;
            if (quad[ i ].y() > y)
                wholetop = false;
        }
        if (wholetop || wholebottom) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.top() == quad.bottom()) { // quad has no size
            ret.append(quad);
            continue;
        }
        ret.append(quad.makeSubQuad(quad.left(), quad.top(), quad.right(), y));
        ret.append(quad.makeSubQuad(quad.left(), y, quad.right(), quad.bottom()));
    }
    return ret;
}

WindowQuadList WindowQuadList::makeGrid(int maxQuadSize) const
{
    if (empty())
        return *this;

    // Find the bounding rectangle
    double left   = first().left();
    double right  = first().right();
    double top    = first().top();
    double bottom = first().bottom();

    foreach (const WindowQuad &quad, *this) {
#ifndef NDEBUG
        if (quad.isTransformed())
            kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        left   = qMin(left,   quad.left());
        right  = qMax(right,  quad.right());
        top    = qMin(top,    quad.top());
        bottom = qMax(bottom, quad.bottom());
    }

    WindowQuadList ret;

    foreach (const WindowQuad &quad, *this) {
        const double quadLeft   = quad.left();
        const double quadRight  = quad.right();
        const double quadTop    = quad.top();
        const double quadBottom = quad.bottom();

        // Compute the top-left corner of the first intersecting grid cell
        const double xBegin = left + qFloor((quadLeft - left) / maxQuadSize) * maxQuadSize;
        const double yBegin = top  + qFloor((quadTop  - top)  / maxQuadSize) * maxQuadSize;

        // Loop over all intersecting cells and add sub-quads
        for (double y = yBegin; y < quadBottom; y += maxQuadSize) {
            const double y0 = qMax(y, quadTop);
            const double y1 = qMin(quadBottom, y + maxQuadSize);

            for (double x = xBegin; x < quadRight; x += maxQuadSize) {
                const double x0 = qMax(x, quadLeft);
                const double x1 = qMin(quadRight, x + maxQuadSize);

                ret.append(quad.makeSubQuad(x0, y0, x1, y1));
            }
        }
    }

    return ret;
}

WindowQuadList WindowQuadList::makeRegularGrid(int xSubdivisions, int ySubdivisions) const
{
    if (empty())
        return *this;

    // Find the bounding rectangle
    double left   = first().left();
    double right  = first().right();
    double top    = first().top();
    double bottom = first().bottom();

    foreach (const WindowQuad &quad, *this) {
#ifndef NDEBUG
        if (quad.isTransformed())
            kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!";
#endif
        left   = qMin(left,   quad.left());
        right  = qMax(right,  quad.right());
        top    = qMin(top,    quad.top());
        bottom = qMax(bottom, quad.bottom());
    }

    double xIncrement = (right - left) / xSubdivisions;
    double yIncrement = (bottom - top) / ySubdivisions;

    WindowQuadList ret;

    foreach (const WindowQuad &quad, *this) {
        const double quadLeft   = quad.left();
        const double quadRight  = quad.right();
        const double quadTop    = quad.top();
        const double quadBottom = quad.bottom();

        // Compute the top-left corner of the first intersecting grid cell
        const double xBegin = left + qFloor((quadLeft - left) / xIncrement) * xIncrement;
        const double yBegin = top  + qFloor((quadTop  - top)  / yIncrement) * yIncrement;

        // Loop over all intersecting cells and add sub-quads
        for (double y = yBegin; y < quadBottom; y += yIncrement) {
            const double y0 = qMax(y, quadTop);
            const double y1 = qMin(quadBottom, y + yIncrement);

            for (double x = xBegin; x < quadRight; x += xIncrement) {
                const double x0 = qMax(x, quadLeft);
                const double x1 = qMin(quadRight, x + xIncrement);

                ret.append(quad.makeSubQuad(x0, y0, x1, y1));
            }
        }
    }

    return ret;
}

#ifndef GL_TRIANGLES
#  define GL_TRIANGLES      0x0004
#endif

#ifndef GL_QUADS
#  define GL_QUADS          0x0007
#endif

void WindowQuadList::makeInterleavedArrays(unsigned int type, GLVertex2D *vertices, const QMatrix4x4 &textureMatrix) const
{
    // Since we know that the texture matrix just scales and translates
    // we can use this information to optimize the transformation
    const QVector2D coeff(textureMatrix(0, 0), textureMatrix(1, 1));
    const QVector2D offset(textureMatrix(0, 3), textureMatrix(1, 3));

    GLVertex2D *vertex = vertices;

    assert(type == GL_QUADS || type == GL_TRIANGLES);

    switch (type)
    {
    case GL_QUADS:
#ifdef HAVE_SSE2
        if (!(intptr_t(vertex) & 0xf)) {
            for (int i = 0; i < count(); i++) {
                const WindowQuad &quad = at(i);
                KWIN_ALIGN(16) GLVertex2D v[4];

                for (int j = 0; j < 4; j++) {
                    const WindowVertex &wv = quad[j];

                    v[j].position = QVector2D(wv.x(), wv.y());
                    v[j].texcoord = QVector2D(wv.u(), wv.v()) * coeff + offset;
                }

                const __m128i *srcP = (const __m128i *) &v;
                __m128i *dstP = (__m128i *) vertex;

                _mm_stream_si128(&dstP[0], _mm_load_si128(&srcP[0])); // Top-left
                _mm_stream_si128(&dstP[1], _mm_load_si128(&srcP[1])); // Top-right
                _mm_stream_si128(&dstP[2], _mm_load_si128(&srcP[2])); // Bottom-right
                _mm_stream_si128(&dstP[3], _mm_load_si128(&srcP[3])); // Bottom-left

                vertex += 4;
            }
        } else
#endif // HAVE_SSE2
        {
            for (int i = 0; i < count(); i++) {
                const WindowQuad &quad = at(i);

                for (int j = 0; j < 4; j++) {
                    const WindowVertex &wv = quad[j];

                    GLVertex2D v;
                    v.position = QVector2D(wv.x(), wv.y());
                    v.texcoord = QVector2D(wv.u(), wv.v()) * coeff + offset;

                    *(vertex++) = v;
                }
            }
        }
        break;

    case GL_TRIANGLES:
#ifdef HAVE_SSE2
        if (!(intptr_t(vertex) & 0xf)) {
            for (int i = 0; i < count(); i++) {
                const WindowQuad &quad = at(i);
                KWIN_ALIGN(16) GLVertex2D v[4];

                for (int j = 0; j < 4; j++) {
                    const WindowVertex &wv = quad[j];

                    v[j].position = QVector2D(wv.x(), wv.y());
                    v[j].texcoord = QVector2D(wv.u(), wv.v()) * coeff + offset;
                }

                const __m128i *srcP = (const __m128i *) &v;
                __m128i *dstP = (__m128i *) vertex;

                __m128i src[4];
                src[0] = _mm_load_si128(&srcP[0]); // Top-left
                src[1] = _mm_load_si128(&srcP[1]); // Top-right
                src[2] = _mm_load_si128(&srcP[2]); // Bottom-right
                src[3] = _mm_load_si128(&srcP[3]); // Bottom-left

                // First triangle
                _mm_stream_si128(&dstP[0], src[1]); // Top-right
                _mm_stream_si128(&dstP[1], src[0]); // Top-left
                _mm_stream_si128(&dstP[2], src[3]); // Bottom-left

                // Second triangle
                _mm_stream_si128(&dstP[3], src[3]); // Bottom-left
                _mm_stream_si128(&dstP[4], src[2]); // Bottom-right
                _mm_stream_si128(&dstP[5], src[1]); // Top-right

                vertex += 6;
            }
        } else
#endif // HAVE_SSE2
        {
            for (int i = 0; i < count(); i++) {
                const WindowQuad &quad = at(i);
                GLVertex2D v[4]; // Four unique vertices / quad

                for (int j = 0; j < 4; j++) {
                    const WindowVertex &wv = quad[j];

                    v[j].position = QVector2D(wv.x(), wv.y());
                    v[j].texcoord = QVector2D(wv.u(), wv.v()) * coeff + offset;
                }

                // First triangle
                *(vertex++) = v[1]; // Top-right
                *(vertex++) = v[0]; // Top-left
                *(vertex++) = v[3]; // Bottom-left

                // Second triangle
                *(vertex++) = v[3]; // Bottom-left
                *(vertex++) = v[2]; // Bottom-right
                *(vertex++) = v[1]; // Top-right
            }
        }
        break;

    default:
        break;
    }
}

void WindowQuadList::makeArrays(float **vertices, float **texcoords, const QSizeF &size, bool yInverted) const
{
    *vertices = new float[count() * 6 * 2];
    *texcoords = new float[count() * 6 * 2];

    float *vpos = *vertices;
    float *tpos = *texcoords;

     // Note: The positions in a WindowQuad are stored in clockwise order
    const int index[] = { 1, 0, 3, 3, 2, 1 };

    for (int i = 0; i < count(); i++) {
        const WindowQuad &quad = at(i);

        for (int j = 0; j < 6; j++) {
            const WindowVertex &wv = quad[index[j]];

            *vpos++ = wv.x();
            *vpos++ = wv.y();

            *tpos++ = wv.u() / size.width();
            *tpos++ = yInverted ? (wv.v() / size.height()) : (1.0 - wv.v() / size.height());
        }
    }
}

WindowQuadList WindowQuadList::select(WindowQuadType type) const
{
    foreach (const WindowQuad & q, *this) {
        if (q.type() != type) { // something else than ones to select, make a copy and filter
            WindowQuadList ret;
            foreach (const WindowQuad & q, *this) {
                if (q.type() == type)
                    ret.append(q);
            }
            return ret;
        }
    }
    return *this; // nothing to filter out
}

WindowQuadList WindowQuadList::filterOut(WindowQuadType type) const
{
    foreach (const WindowQuad & q, *this) {
        if (q.type() == type) { // something to filter out, make a copy and filter
            WindowQuadList ret;
            foreach (const WindowQuad & q, *this) {
                if (q.type() != type)
                    ret.append(q);
            }
            return ret;
        }
    }
    return *this; // nothing to filter out
}

bool WindowQuadList::smoothNeeded() const
{
    foreach (const WindowQuad & q, *this)
    if (q.smoothNeeded())
        return true;
    return false;
}

bool WindowQuadList::isTransformed() const
{
    foreach (const WindowQuad & q, *this)
    if (q.isTransformed())
        return true;
    return false;
}

/***************************************************************
 PaintClipper
***************************************************************/

QStack< QRegion >* PaintClipper::areas = NULL;

PaintClipper::PaintClipper(const QRegion& allowed_area)
    : area(allowed_area)
{
    push(area);
}

PaintClipper::~PaintClipper()
{
    pop(area);
}

void PaintClipper::push(const QRegion& allowed_area)
{
    if (allowed_area == infiniteRegion())  // don't push these
        return;
    if (areas == NULL)
        areas = new QStack< QRegion >;
    areas->push(allowed_area);
}

void PaintClipper::pop(const QRegion& allowed_area)
{
    if (allowed_area == infiniteRegion())
        return;
    Q_ASSERT(areas != NULL);
    Q_ASSERT(areas->top() == allowed_area);
    areas->pop();
    if (areas->isEmpty()) {
        delete areas;
        areas = NULL;
    }
}

bool PaintClipper::clip()
{
    return areas != NULL;
}

QRegion PaintClipper::paintArea()
{
    assert(areas != NULL);   // can be called only with clip() == true
    QRegion ret = QRegion(0, 0, displayWidth(), displayHeight());
    foreach (const QRegion & r, *areas)
    ret &= r;
    return ret;
}

struct PaintClipper::Iterator::Data {
    Data() : index(0) {}
    int index;
    QVector< QRect > rects;
};

PaintClipper::Iterator::Iterator()
    : data(new Data)
{
    if (clip() && effects->isOpenGLCompositing()) {
        data->rects = paintArea().rects();
        data->index = -1;
        next(); // move to the first one
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (clip() && effects->compositingType() == XRenderCompositing) {
        XFixesRegion region(paintArea());
        xcb_xfixes_set_picture_clip_region(connection(), effects->xrenderBufferPicture(), region, 0, 0);
    }
#endif
}

PaintClipper::Iterator::~Iterator()
{
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (clip() && effects->compositingType() == XRenderCompositing)
        xcb_xfixes_set_picture_clip_region(connection(), effects->xrenderBufferPicture(), XCB_XFIXES_REGION_NONE, 0, 0);
#endif
    delete data;
}

bool PaintClipper::Iterator::isDone()
{
    if (!clip())
        return data->index == 1; // run once
    if (effects->isOpenGLCompositing())
        return data->index >= data->rects.count(); // run once per each area
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing)
        return data->index == 1; // run once
#endif
    abort();
}

void PaintClipper::Iterator::next()
{
    data->index++;
}

QRect PaintClipper::Iterator::boundingRect() const
{
    if (!clip())
        return infiniteRegion();
    if (effects->isOpenGLCompositing())
        return data->rects[ data->index ];
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing)
        return paintArea().boundingRect();
#endif
    abort();
    return infiniteRegion();
}

/***************************************************************
 Motion1D
***************************************************************/

Motion1D::Motion1D(double initial, double strength, double smoothness)
    : Motion<double>(initial, strength, smoothness)
{
}

Motion1D::Motion1D(const Motion1D &other)
    : Motion<double>(other)
{
}

Motion1D::~Motion1D()
{
}

/***************************************************************
 Motion2D
***************************************************************/

Motion2D::Motion2D(QPointF initial, double strength, double smoothness)
    : Motion<QPointF>(initial, strength, smoothness)
{
}

Motion2D::Motion2D(const Motion2D &other)
    : Motion<QPointF>(other)
{
}

Motion2D::~Motion2D()
{
}

/***************************************************************
 WindowMotionManager
***************************************************************/

WindowMotionManager::WindowMotionManager(bool useGlobalAnimationModifier)
    :   m_useGlobalAnimationModifier(useGlobalAnimationModifier)

{
    // TODO: Allow developer to modify motion attributes
} // TODO: What happens when the window moves by an external force?

WindowMotionManager::~WindowMotionManager()
{
}

void WindowMotionManager::manage(EffectWindow *w)
{
    if (m_managedWindows.contains(w))
        return;

    double strength = 0.08;
    double smoothness = 4.0;
    if (m_useGlobalAnimationModifier && effects->animationTimeFactor()) {
        // If the factor is == 0 then we just skip the calculation completely
        strength = 0.08 / effects->animationTimeFactor();
        smoothness = effects->animationTimeFactor() * 4.0;
    }

    WindowMotion &motion = m_managedWindows[ w ];
    motion.translation.setStrength(strength);
    motion.translation.setSmoothness(smoothness);
    motion.scale.setStrength(strength * 1.33);
    motion.scale.setSmoothness(smoothness / 2.0);

    motion.translation.setValue(w->pos());
    motion.scale.setValue(QPointF(1.0, 1.0));
}

void WindowMotionManager::unmanage(EffectWindow *w)
{
    m_movingWindowsSet.remove(w);
    m_managedWindows.remove(w);
}

void WindowMotionManager::unmanageAll()
{
    m_managedWindows.clear();
    m_movingWindowsSet.clear();
}

void WindowMotionManager::calculate(int time)
{
    if (!effects->animationTimeFactor()) {
        // Just skip it completely if the user wants no animation
        m_movingWindowsSet.clear();
        QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.begin();
        for (; it != m_managedWindows.end(); ++it) {
            WindowMotion *motion = &it.value();
            motion->translation.finish();
            motion->scale.finish();
        }
    }

    QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.begin();
    for (; it != m_managedWindows.end(); ++it) {
        WindowMotion *motion = &it.value();
        int stopped = 0;

        // TODO: What happens when distance() == 0 but we are still moving fast?
        // TODO: Motion needs to be calculated from the window's center

        Motion2D *trans = &motion->translation;
        if (trans->distance().isNull())
            ++stopped;
        else {
            // Still moving
            trans->calculate(time);
            const short fx = trans->target().x() <= trans->startValue().x() ? -1 : 1;
            const short fy = trans->target().y() <= trans->startValue().y() ? -1 : 1;
            if (trans->distance().x()*fx/0.5 < 1.0 && trans->velocity().x()*fx/0.2 < 1.0 &&
                trans->distance().y()*fy/0.5 < 1.0 && trans->velocity().y()*fy/0.2 < 1.0) {
                // Hide tiny oscillations
                motion->translation.finish();
                ++stopped;
            }
        }

        Motion2D *scale = &motion->scale;
        if (scale->distance().isNull())
            ++stopped;
        else {
            // Still scaling
            scale->calculate(time);
            const short fx = scale->target().x() < 1.0 ? -1 : 1;
            const short fy = scale->target().y() < 1.0 ? -1 : 1;
            if (scale->distance().x()*fx/0.001 < 1.0 && scale->velocity().x()*fx/0.05 < 1.0 &&
                scale->distance().y()*fy/0.001 < 1.0 && scale->velocity().y()*fy/0.05 < 1.0) {
                // Hide tiny oscillations
                motion->scale.finish();
                ++stopped;
            }
        }

        // We just finished this window's motion
        if (stopped == 2)
            m_movingWindowsSet.remove(it.key());
    }
}

void WindowMotionManager::reset()
{
    QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.begin();
    for (; it != m_managedWindows.end(); ++it) {
        WindowMotion *motion = &it.value();
        EffectWindow *window = it.key();
        motion->translation.setTarget(window->pos());
        motion->translation.finish();
        motion->scale.setTarget(QPointF(1.0, 1.0));
        motion->scale.finish();
    }
}

void WindowMotionManager::reset(EffectWindow *w)
{
    QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end())
        return;

    WindowMotion *motion = &it.value();
    motion->translation.setTarget(w->pos());
    motion->translation.finish();
    motion->scale.setTarget(QPointF(1.0, 1.0));
    motion->scale.finish();
}

void WindowMotionManager::apply(EffectWindow *w, WindowPaintData &data)
{
    QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end())
        return;

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    WindowMotion *motion = &it.value();
    data += (motion->translation.value() - QPointF(w->x(), w->y()));
    data *= QVector2D(motion->scale.value());
}

void WindowMotionManager::moveWindow(EffectWindow *w, QPoint target, double scale, double yScale)
{
    QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end())
        abort(); // Notify the effect author that they did something wrong

    WindowMotion *motion = &it.value();

    if (yScale == 0.0)
        yScale = scale;
    QPointF scalePoint(scale, yScale);

    if (motion->translation.value() == target && motion->scale.value() == scalePoint)
        return; // Window already at that position

    motion->translation.setTarget(target);
    motion->scale.setTarget(scalePoint);

    m_movingWindowsSet << w;
}

QRectF WindowMotionManager::transformedGeometry(EffectWindow *w) const
{
    QHash<EffectWindow*, WindowMotion>::const_iterator it = m_managedWindows.constFind(w);
    if (it == m_managedWindows.end())
        return w->geometry();

    const WindowMotion *motion = &it.value();
    QRectF geometry(w->geometry());

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    geometry.moveTo(motion->translation.value());
    geometry.setWidth(geometry.width() * motion->scale.value().x());
    geometry.setHeight(geometry.height() * motion->scale.value().y());

    return geometry;
}

void WindowMotionManager::setTransformedGeometry(EffectWindow *w, const QRectF &geometry)
{
    QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end())
        return;
    WindowMotion *motion = &it.value();
    motion->translation.setValue(geometry.topLeft());
    motion->scale.setValue(QPointF(geometry.width() / qreal(w->width()), geometry.height() / qreal(w->height())));
}

QRectF WindowMotionManager::targetGeometry(EffectWindow *w) const
{
    QHash<EffectWindow*, WindowMotion>::const_iterator it = m_managedWindows.constFind(w);
    if (it == m_managedWindows.end())
        return w->geometry();

    const WindowMotion *motion = &it.value();
    QRectF geometry(w->geometry());

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    geometry.moveTo(motion->translation.target());
    geometry.setWidth(geometry.width() * motion->scale.target().x());
    geometry.setHeight(geometry.height() * motion->scale.target().y());

    return geometry;
}

EffectWindow* WindowMotionManager::windowAtPoint(QPoint point, bool useStackingOrder) const
{
    Q_UNUSED(useStackingOrder);
    // TODO: Stacking order uses EffectsHandler::stackingOrder() then filters by m_managedWindows
    QHash< EffectWindow*, WindowMotion >::ConstIterator it = m_managedWindows.constBegin();
    while (it != m_managedWindows.constEnd()) {
        if (transformedGeometry(it.key()).contains(point))
            return it.key();
        ++it;
    }

    return NULL;
}

/***************************************************************
 EffectFramePrivate
***************************************************************/
class EffectFramePrivate
{
public:
    EffectFramePrivate();
    ~EffectFramePrivate();

    bool crossFading;
    qreal crossFadeProgress;
};

EffectFramePrivate::EffectFramePrivate()
    : crossFading(false)
    , crossFadeProgress(1.0)
{
}

EffectFramePrivate::~EffectFramePrivate()
{
}

/***************************************************************
 EffectFrame
***************************************************************/
EffectFrame::EffectFrame()
    : d(new EffectFramePrivate)
{
}

EffectFrame::~EffectFrame()
{
    delete d;
}

qreal EffectFrame::crossFadeProgress() const
{
    return d->crossFadeProgress;
}

void EffectFrame::setCrossFadeProgress(qreal progress)
{
    d->crossFadeProgress = progress;
}

bool EffectFrame::isCrossFade() const
{
    return d->crossFading;
}

void EffectFrame::enableCrossFade(bool enable)
{
    d->crossFading = enable;
}

} // namespace

#include "kwineffects.moc"
