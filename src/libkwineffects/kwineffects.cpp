/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwineffects.h"

#include "config-kwin.h"

#include <QFontMetrics>
#include <QMatrix4x4>
#include <QPainter>
#include <QPixmap>
#include <QTimeLine>
#include <QVariant>
#include <QWindow>
#include <QtMath>

#include <kconfiggroup.h>
#include <ksharedconfig.h>

#include <optional>

namespace KWin
{

void WindowPrePaintData::setTranslucent()
{
    mask |= Effect::PAINT_WINDOW_TRANSLUCENT;
    mask &= ~Effect::PAINT_WINDOW_OPAQUE;
    opaque = QRegion(); // cannot clip, will be transparent
}

void WindowPrePaintData::setTransformed()
{
    mask |= Effect::PAINT_WINDOW_TRANSFORMED;
}

class PaintDataPrivate
{
public:
    PaintDataPrivate()
        : scale(1., 1., 1.)
        , rotationAxis(0, 0, 1.)
        , rotationAngle(0.)
    {
    }
    QVector3D scale;
    QVector3D translation;

    QVector3D rotationAxis;
    QVector3D rotationOrigin;
    qreal rotationAngle;
};

PaintData::PaintData()
    : d(std::make_unique<PaintDataPrivate>())
{
}

PaintData::~PaintData() = default;

qreal PaintData::xScale() const
{
    return d->scale.x();
}

qreal PaintData::yScale() const
{
    return d->scale.y();
}

qreal PaintData::zScale() const
{
    return d->scale.z();
}

void PaintData::setScale(const QVector2D &scale)
{
    d->scale.setX(scale.x());
    d->scale.setY(scale.y());
}

void PaintData::setScale(const QVector3D &scale)
{
    d->scale = scale;
}
void PaintData::setXScale(qreal scale)
{
    d->scale.setX(scale);
}

void PaintData::setYScale(qreal scale)
{
    d->scale.setY(scale);
}

void PaintData::setZScale(qreal scale)
{
    d->scale.setZ(scale);
}

const QVector3D &PaintData::scale() const
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
    return d->rotationAngle;
}

QVector3D PaintData::rotationAxis() const
{
    return d->rotationAxis;
}

QVector3D PaintData::rotationOrigin() const
{
    return d->rotationOrigin;
}

void PaintData::setRotationAngle(qreal angle)
{
    d->rotationAngle = angle;
}

void PaintData::setRotationAxis(Qt::Axis axis)
{
    switch (axis) {
    case Qt::XAxis:
        setRotationAxis(QVector3D(1, 0, 0));
        break;
    case Qt::YAxis:
        setRotationAxis(QVector3D(0, 1, 0));
        break;
    case Qt::ZAxis:
        setRotationAxis(QVector3D(0, 0, 1));
        break;
    }
}

void PaintData::setRotationAxis(const QVector3D &axis)
{
    d->rotationAxis = axis;
}

void PaintData::setRotationOrigin(const QVector3D &origin)
{
    d->rotationOrigin = origin;
}

QMatrix4x4 PaintData::toMatrix(qreal deviceScale) const
{
    QMatrix4x4 ret;
    if (d->translation != QVector3D(0, 0, 0)) {
        ret.translate(d->translation * deviceScale);
    }
    if (d->scale != QVector3D(1, 1, 1)) {
        ret.scale(d->scale);
    }

    if (d->rotationAngle != 0) {
        ret.translate(d->rotationOrigin * deviceScale);
        ret.rotate(d->rotationAngle, d->rotationAxis);
        ret.translate(-d->rotationOrigin * deviceScale);
    }

    return ret;
}

class WindowPaintDataPrivate
{
public:
    qreal opacity;
    qreal saturation;
    qreal brightness;
    int screen;
    qreal crossFadeProgress;
    QMatrix4x4 projectionMatrix;
    std::optional<qreal> renderTargetScale = std::nullopt;
};

WindowPaintData::WindowPaintData()
    : WindowPaintData(QMatrix4x4())
{
}

WindowPaintData::WindowPaintData(const QMatrix4x4 &projectionMatrix)
    : PaintData()
    , d(std::make_unique<WindowPaintDataPrivate>())
{
    setProjectionMatrix(projectionMatrix);
    setOpacity(1.0);
    setSaturation(1.0);
    setBrightness(1.0);
    setScreen(0);
    setCrossFadeProgress(0.0);
}

WindowPaintData::WindowPaintData(const WindowPaintData &other)
    : PaintData()
    , d(std::make_unique<WindowPaintDataPrivate>())
{
    setXScale(other.xScale());
    setYScale(other.yScale());
    setZScale(other.zScale());
    translate(other.translation());
    setRotationOrigin(other.rotationOrigin());
    setRotationAxis(other.rotationAxis());
    setRotationAngle(other.rotationAngle());
    setOpacity(other.opacity());
    setSaturation(other.saturation());
    setBrightness(other.brightness());
    setScreen(other.screen());
    setCrossFadeProgress(other.crossFadeProgress());
    setProjectionMatrix(other.projectionMatrix());
}

WindowPaintData::~WindowPaintData() = default;

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
    d->crossFadeProgress = std::clamp(factor, 0.0, 1.0);
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

void WindowPaintData::setProjectionMatrix(const QMatrix4x4 &matrix)
{
    d->projectionMatrix = matrix;
}

QMatrix4x4 WindowPaintData::projectionMatrix() const
{
    return d->projectionMatrix;
}

QMatrix4x4 &WindowPaintData::rprojectionMatrix()
{
    return d->projectionMatrix;
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

std::optional<qreal> WindowPaintData::renderTargetScale() const
{
    return d->renderTargetScale;
}

void WindowPaintData::setRenderTargetScale(qreal scale)
{
    d->renderTargetScale = scale;
}

class ScreenPaintData::Private
{
public:
    QMatrix4x4 projectionMatrix;
    EffectScreen *screen = nullptr;
};

ScreenPaintData::ScreenPaintData()
    : d(new Private())
{
}

ScreenPaintData::ScreenPaintData(const QMatrix4x4 &projectionMatrix, EffectScreen *screen)
    : d(new Private())
{
    d->projectionMatrix = projectionMatrix;
    d->screen = screen;
}

ScreenPaintData::~ScreenPaintData() = default;

ScreenPaintData::ScreenPaintData(const ScreenPaintData &other)
    : d(new Private())
{
    d->projectionMatrix = other.d->projectionMatrix;
    d->screen = other.d->screen;
}

ScreenPaintData &ScreenPaintData::operator=(const ScreenPaintData &rhs)
{
    d->projectionMatrix = rhs.d->projectionMatrix;
    d->screen = rhs.d->screen;
    return *this;
}

QMatrix4x4 ScreenPaintData::projectionMatrix() const
{
    return d->projectionMatrix;
}

EffectScreen *ScreenPaintData::screen() const
{
    return d->screen;
}

//****************************************
// Effect
//****************************************

Effect::Effect(QObject *parent)
    : QObject(parent)
{
}

Effect::~Effect()
{
}

void Effect::reconfigure(ReconfigureFlags)
{
}

void *Effect::proxy()
{
    return nullptr;
}

void Effect::windowInputMouseEvent(QEvent *)
{
}

void Effect::grabbedKeyboardEvent(QKeyEvent *)
{
}

bool Effect::borderActivated(ElectricBorder)
{
    return false;
}

void Effect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    effects->prePaintScreen(data, presentTime);
}

void Effect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
}

void Effect::postPaintScreen()
{
    effects->postPaintScreen();
}

void Effect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    effects->prePaintWindow(w, data, presentTime);
}

void Effect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    effects->paintWindow(w, mask, region, data);
}

void Effect::postPaintWindow(EffectWindow *w)
{
    effects->postPaintWindow(w);
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

void Effect::drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    effects->drawWindow(w, mask, region, data);
}

void Effect::setPositionTransformations(WindowPaintData &data, QRect &region, EffectWindow *w,
                                        const QRect &r, Qt::AspectRatioMode aspect)
{
    QSizeF size = w->size();
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

QPoint Effect::cursorPos()
{
    return effects->cursorPos();
}

double Effect::animationTime(const KConfigGroup &cfg, const QString &key, int defaultTime)
{
    int time = cfg.readEntry(key, 0);
    return time != 0 ? time : std::max(defaultTime * effects->animationTimeFactor(), 1.);
}

double Effect::animationTime(int defaultTime)
{
    // at least 1ms, otherwise 0ms times can break some things
    return std::max(defaultTime * effects->animationTimeFactor(), 1.);
}

int Effect::requestedEffectChainPosition() const
{
    return 0;
}

xcb_connection_t *Effect::xcbConnection() const
{
    return effects->xcbConnection();
}

xcb_window_t Effect::x11RootWindow() const
{
    return effects->x11RootWindow();
}

bool Effect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    return false;
}

bool Effect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    return false;
}

bool Effect::touchUp(qint32 id, std::chrono::microseconds time)
{
    return false;
}

bool Effect::perform(Feature feature, const QVariantList &arguments)
{
    return false;
}

bool Effect::tabletToolEvent(QTabletEvent *event)
{
    return false;
}

bool Effect::tabletToolButtonEvent(uint button, bool pressed, quint64 tabletToolId)
{
    return false;
}

bool Effect::tabletPadButtonEvent(uint button, bool pressed, void *tabletPadId)
{
    return false;
}

bool Effect::tabletPadStripEvent(int number, int position, bool isFinger, void *tabletPadId)
{
    return false;
}

bool Effect::tabletPadRingEvent(int number, int position, bool isFinger, void *tabletPadId)
{
    return false;
}

bool Effect::blocksDirectScanout() const
{
    return true;
}

//****************************************
// EffectFactory
//****************************************
EffectPluginFactory::EffectPluginFactory()
{
}

EffectPluginFactory::~EffectPluginFactory()
{
}

bool EffectPluginFactory::enabledByDefault() const
{
    return true;
}

bool EffectPluginFactory::isSupported() const
{
    return true;
}

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler(CompositingType type)
    : compositing_type(type)
{
    if (compositing_type == NoCompositing) {
        return;
    }
    KWin::effects = this;
    connect(this, QOverload<int, int>::of(&EffectsHandler::desktopChanged), this, &EffectsHandler::desktopChangedLegacy);
}

EffectsHandler::~EffectsHandler()
{
    // All effects should already be unloaded by Impl dtor
    Q_ASSERT(loaded_effects.count() == 0);
    KWin::effects = nullptr;
}

CompositingType EffectsHandler::compositingType() const
{
    return compositing_type;
}

bool EffectsHandler::isOpenGLCompositing() const
{
    return compositing_type & OpenGLCompositing;
}

QRectF EffectsHandler::mapToRenderTarget(const QRectF &rect) const
{
    const QRectF targetRect = renderTargetRect();
    const qreal targetScale = renderTargetScale();

    return QRectF((rect.x() - targetRect.x()) * targetScale,
                  (rect.y() - targetRect.y()) * targetScale,
                  rect.width() * targetScale,
                  rect.height() * targetScale);
}

QRegion EffectsHandler::mapToRenderTarget(const QRegion &region) const
{
    QRegion result;
    for (const QRect &rect : region) {
        result += mapToRenderTarget(QRectF(rect)).toRect();
    }
    return result;
}

EffectsHandler *effects = nullptr;

EffectScreen::EffectScreen(QObject *parent)
    : QObject(parent)
{
}

QPointF EffectScreen::mapToGlobal(const QPointF &pos) const
{
    return pos + geometry().topLeft();
}

QPointF EffectScreen::mapFromGlobal(const QPointF &pos) const
{
    return pos - geometry().topLeft();
}

//****************************************
// EffectWindow
//****************************************

class Q_DECL_HIDDEN EffectWindow::Private
{
public:
    Private(EffectWindow *q);

    EffectWindow *q;
};

EffectWindow::Private::Private(EffectWindow *q)
    : q(q)
{
}

EffectWindow::EffectWindow()
    : d(new Private(this))
{
}

EffectWindow::~EffectWindow()
{
}

bool EffectWindow::isOnActivity(const QString &activity) const
{
    const QStringList _activities = activities();
    return _activities.isEmpty() || _activities.contains(activity);
}

bool EffectWindow::isOnAllActivities() const
{
    return activities().isEmpty();
}

void EffectWindow::setMinimized(bool min)
{
    if (min) {
        minimize();
    } else {
        unminimize();
    }
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
    const QVector<uint> ds = desktops();
    return ds.isEmpty() || ds.contains(d);
}

bool EffectWindow::isOnAllDesktops() const
{
    return desktops().isEmpty();
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

/***************************************************************
 WindowQuad
***************************************************************/

WindowQuad WindowQuad::makeSubQuad(double x1, double y1, double x2, double y2) const
{
    Q_ASSERT(x1 < x2 && y1 < y2 && x1 >= left() && x2 <= right() && y1 >= top() && y2 <= bottom());
    WindowQuad ret(*this);
    // vertices are clockwise starting from topleft
    ret.verts[0].px = x1;
    ret.verts[3].px = x1;
    ret.verts[1].px = x2;
    ret.verts[2].px = x2;
    ret.verts[0].py = y1;
    ret.verts[1].py = y1;
    ret.verts[2].py = y2;
    ret.verts[3].py = y2;

    const double xOrigin = left();
    const double yOrigin = top();

    const double widthReciprocal = 1 / (right() - xOrigin);
    const double heightReciprocal = 1 / (bottom() - yOrigin);

    for (int i = 0; i < 4; ++i) {
        const double w1 = (ret.verts[i].px - xOrigin) * widthReciprocal;
        const double w2 = (ret.verts[i].py - yOrigin) * heightReciprocal;

        // Use bilinear interpolation to compute the texture coords.
        ret.verts[i].tx = (1 - w1) * (1 - w2) * verts[0].tx + w1 * (1 - w2) * verts[1].tx + w1 * w2 * verts[2].tx + (1 - w1) * w2 * verts[3].tx;
        ret.verts[i].ty = (1 - w1) * (1 - w2) * verts[0].ty + w1 * (1 - w2) * verts[1].ty + w1 * w2 * verts[2].ty + (1 - w1) * w2 * verts[3].ty;
    }

    return ret;
}

/***************************************************************
 WindowQuadList
***************************************************************/

WindowQuadList WindowQuadList::splitAtX(double x) const
{
    WindowQuadList ret;
    ret.reserve(count());
    for (const WindowQuad &quad : *this) {
        bool wholeleft = true;
        bool wholeright = true;
        for (int i = 0; i < 4; ++i) {
            if (quad[i].x() < x) {
                wholeright = false;
            }
            if (quad[i].x() > x) {
                wholeleft = false;
            }
        }
        if (wholeleft || wholeright) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.top() == quad.bottom() || quad.left() == quad.right()) { // quad has no size
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
    ret.reserve(count());
    for (const WindowQuad &quad : *this) {
        bool wholetop = true;
        bool wholebottom = true;
        for (int i = 0; i < 4; ++i) {
            if (quad[i].y() < y) {
                wholebottom = false;
            }
            if (quad[i].y() > y) {
                wholetop = false;
            }
        }
        if (wholetop || wholebottom) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.top() == quad.bottom() || quad.left() == quad.right()) { // quad has no size
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
    if (empty()) {
        return *this;
    }

    // Find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();

    for (const WindowQuad &quad : std::as_const(*this)) {
        left = std::min(left, quad.left());
        right = std::max(right, quad.right());
        top = std::min(top, quad.top());
        bottom = std::max(bottom, quad.bottom());
    }

    WindowQuadList ret;

    for (const WindowQuad &quad : std::as_const(*this)) {
        const double quadLeft = quad.left();
        const double quadRight = quad.right();
        const double quadTop = quad.top();
        const double quadBottom = quad.bottom();

        // sanity check, see BUG 390953
        if (quadLeft == quadRight || quadTop == quadBottom) {
            ret.append(quad);
            continue;
        }

        // Compute the top-left corner of the first intersecting grid cell
        const double xBegin = left + qFloor((quadLeft - left) / maxQuadSize) * maxQuadSize;
        const double yBegin = top + qFloor((quadTop - top) / maxQuadSize) * maxQuadSize;

        // Loop over all intersecting cells and add sub-quads
        for (double y = yBegin; y < quadBottom; y += maxQuadSize) {
            const double y0 = std::max(y, quadTop);
            const double y1 = std::min(quadBottom, y + maxQuadSize);

            for (double x = xBegin; x < quadRight; x += maxQuadSize) {
                const double x0 = std::max(x, quadLeft);
                const double x1 = std::min(quadRight, x + maxQuadSize);

                ret.append(quad.makeSubQuad(x0, y0, x1, y1));
            }
        }
    }

    return ret;
}

WindowQuadList WindowQuadList::makeRegularGrid(int xSubdivisions, int ySubdivisions) const
{
    if (empty()) {
        return *this;
    }

    // Find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();

    for (const WindowQuad &quad : *this) {
        left = std::min(left, quad.left());
        right = std::max(right, quad.right());
        top = std::min(top, quad.top());
        bottom = std::max(bottom, quad.bottom());
    }

    double xIncrement = (right - left) / xSubdivisions;
    double yIncrement = (bottom - top) / ySubdivisions;

    WindowQuadList ret;

    for (const WindowQuad &quad : *this) {
        const double quadLeft = quad.left();
        const double quadRight = quad.right();
        const double quadTop = quad.top();
        const double quadBottom = quad.bottom();

        // sanity check, see BUG 390953
        if (quadLeft == quadRight || quadTop == quadBottom) {
            ret.append(quad);
            continue;
        }

        // Compute the top-left corner of the first intersecting grid cell
        const double xBegin = left + qFloor((quadLeft - left) / xIncrement) * xIncrement;
        const double yBegin = top + qFloor((quadTop - top) / yIncrement) * yIncrement;

        // Loop over all intersecting cells and add sub-quads
        for (double y = yBegin; y < quadBottom; y += yIncrement) {
            const double y0 = std::max(y, quadTop);
            const double y1 = std::min(quadBottom, y + yIncrement);

            for (double x = xBegin; x < quadRight; x += xIncrement) {
                const double x0 = std::max(x, quadLeft);
                const double x1 = std::min(quadRight, x + xIncrement);

                ret.append(quad.makeSubQuad(x0, y0, x1, y1));
            }
        }
    }

    return ret;
}

void RenderGeometry::copy(std::span<GLVertex2D> destination)
{
    Q_ASSERT(int(destination.size()) >= size());
    for (std::size_t i = 0; i < destination.size(); ++i) {
        destination[i] = at(i);
    }
}

void RenderGeometry::appendWindowVertex(const WindowVertex &windowVertex, qreal deviceScale)
{
    GLVertex2D glVertex;
    switch (m_vertexSnappingMode) {
    case VertexSnappingMode::None:
        glVertex.position = QVector2D(windowVertex.x(), windowVertex.y()) * deviceScale;
        break;
    case VertexSnappingMode::Round:
        glVertex.position = roundVector(QVector2D(windowVertex.x(), windowVertex.y()) * deviceScale);
        break;
    }
    glVertex.texcoord = QVector2D(windowVertex.u(), windowVertex.v());
    append(glVertex);
}

void RenderGeometry::appendWindowQuad(const WindowQuad &quad, qreal deviceScale)
{
    // Geometry assumes we're rendering triangles, so add the quad's
    // vertices as two triangles. Vertex order is top-left, bottom-left,
    // top-right followed by top-right, bottom-left, bottom-right.
    appendWindowVertex(quad[0], deviceScale);
    appendWindowVertex(quad[3], deviceScale);
    appendWindowVertex(quad[1], deviceScale);

    appendWindowVertex(quad[1], deviceScale);
    appendWindowVertex(quad[3], deviceScale);
    appendWindowVertex(quad[2], deviceScale);
}

void RenderGeometry::appendSubQuad(const WindowQuad &quad, const QRectF &subquad, qreal deviceScale)
{
    std::array<GLVertex2D, 4> vertices;
    vertices[0].position = QVector2D(subquad.topLeft());
    vertices[1].position = QVector2D(subquad.topRight());
    vertices[2].position = QVector2D(subquad.bottomRight());
    vertices[3].position = QVector2D(subquad.bottomLeft());

    const auto deviceQuad = QRectF{QPointF(std::round(quad.left() * deviceScale), std::round(quad.top() * deviceScale)),
                                   QPointF(std::round(quad.right() * deviceScale), std::round(quad.bottom() * deviceScale))};

    const QPointF origin = deviceQuad.topLeft();
    const QSizeF size = deviceQuad.size();

#pragma GCC unroll 4
    for (int i = 0; i < 4; ++i) {
        const double weight1 = (vertices[i].position.x() - origin.x()) / size.width();
        const double weight2 = (vertices[i].position.y() - origin.y()) / size.height();
        const double oneMinW1 = 1.0 - weight1;
        const double oneMinW2 = 1.0 - weight2;

        const float u = oneMinW1 * oneMinW2 * quad[0].u() + weight1 * oneMinW2 * quad[1].u()
            + weight1 * weight2 * quad[2].u() + oneMinW1 * weight2 * quad[3].u();
        const float v = oneMinW1 * oneMinW2 * quad[0].v() + weight1 * oneMinW2 * quad[1].v()
            + weight1 * weight2 * quad[2].v() + oneMinW1 * weight2 * quad[3].v();
        vertices[i].texcoord = QVector2D(u, v);
    }

    append(vertices[0]);
    append(vertices[3]);
    append(vertices[1]);

    append(vertices[1]);
    append(vertices[3]);
    append(vertices[2]);
}

void RenderGeometry::postProcessTextureCoordinates(const QMatrix4x4 &textureMatrix)
{
    if (!textureMatrix.isIdentity()) {
        const QVector2D coeff(textureMatrix(0, 0), textureMatrix(1, 1));
        const QVector2D offset(textureMatrix(0, 3), textureMatrix(1, 3));

        for (auto &vertex : (*this)) {
            vertex.texcoord = vertex.texcoord * coeff + offset;
        }
    }
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
    : m_useGlobalAnimationModifier(useGlobalAnimationModifier)

{
    // TODO: Allow developer to modify motion attributes
} // TODO: What happens when the window moves by an external force?

WindowMotionManager::~WindowMotionManager()
{
}

void WindowMotionManager::manage(EffectWindow *w)
{
    if (m_managedWindows.contains(w)) {
        return;
    }

    double strength = 0.08;
    double smoothness = 4.0;
    if (m_useGlobalAnimationModifier && effects->animationTimeFactor()) {
        // If the factor is == 0 then we just skip the calculation completely
        strength = 0.08 / effects->animationTimeFactor();
        smoothness = effects->animationTimeFactor() * 4.0;
    }

    WindowMotion &motion = m_managedWindows[w];
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
        QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.begin();
        for (; it != m_managedWindows.end(); ++it) {
            WindowMotion *motion = &it.value();
            motion->translation.finish();
            motion->scale.finish();
        }
    }

    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.begin();
    for (; it != m_managedWindows.end(); ++it) {
        WindowMotion *motion = &it.value();
        int stopped = 0;

        // TODO: What happens when distance() == 0 but we are still moving fast?
        // TODO: Motion needs to be calculated from the window's center

        Motion2D *trans = &motion->translation;
        if (trans->distance().isNull()) {
            ++stopped;
        } else {
            // Still moving
            trans->calculate(time);
            const short fx = trans->target().x() <= trans->startValue().x() ? -1 : 1;
            const short fy = trans->target().y() <= trans->startValue().y() ? -1 : 1;
            if (trans->distance().x() * fx / 0.5 < 1.0 && trans->velocity().x() * fx / 0.2 < 1.0
                && trans->distance().y() * fy / 0.5 < 1.0 && trans->velocity().y() * fy / 0.2 < 1.0) {
                // Hide tiny oscillations
                motion->translation.finish();
                ++stopped;
            }
        }

        Motion2D *scale = &motion->scale;
        if (scale->distance().isNull()) {
            ++stopped;
        } else {
            // Still scaling
            scale->calculate(time);
            const short fx = scale->target().x() < 1.0 ? -1 : 1;
            const short fy = scale->target().y() < 1.0 ? -1 : 1;
            if (scale->distance().x() * fx / 0.001 < 1.0 && scale->velocity().x() * fx / 0.05 < 1.0
                && scale->distance().y() * fy / 0.001 < 1.0 && scale->velocity().y() * fy / 0.05 < 1.0) {
                // Hide tiny oscillations
                motion->scale.finish();
                ++stopped;
            }
        }

        // We just finished this window's motion
        if (stopped == 2) {
            m_movingWindowsSet.remove(it.key());
        }
    }
}

void WindowMotionManager::reset()
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.begin();
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
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return;
    }

    WindowMotion *motion = &it.value();
    motion->translation.setTarget(w->pos());
    motion->translation.finish();
    motion->scale.setTarget(QPointF(1.0, 1.0));
    motion->scale.finish();
}

void WindowMotionManager::apply(EffectWindow *w, WindowPaintData &data)
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return;
    }

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    WindowMotion *motion = &it.value();
    data += (motion->translation.value() - QPointF(w->x(), w->y()));
    data *= QVector2D(motion->scale.value());
}

void WindowMotionManager::moveWindow(EffectWindow *w, QPoint target, double scale, double yScale)
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.find(w);
    Q_ASSERT(it != m_managedWindows.end()); // Notify the effect author that they did something wrong

    WindowMotion *motion = &it.value();

    if (yScale == 0.0) {
        yScale = scale;
    }
    QPointF scalePoint(scale, yScale);

    if (motion->translation.value() == target && motion->scale.value() == scalePoint) {
        return; // Window already at that position
    }

    motion->translation.setTarget(target);
    motion->scale.setTarget(scalePoint);

    m_movingWindowsSet << w;
}

QRectF WindowMotionManager::transformedGeometry(EffectWindow *w) const
{
    QHash<EffectWindow *, WindowMotion>::const_iterator it = m_managedWindows.constFind(w);
    if (it == m_managedWindows.end()) {
        return w->frameGeometry();
    }

    const WindowMotion *motion = &it.value();
    QRectF geometry(w->frameGeometry());

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    geometry.moveTo(motion->translation.value());
    geometry.setWidth(geometry.width() * motion->scale.value().x());
    geometry.setHeight(geometry.height() * motion->scale.value().y());

    return geometry;
}

void WindowMotionManager::setTransformedGeometry(EffectWindow *w, const QRectF &geometry)
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return;
    }
    WindowMotion *motion = &it.value();
    motion->translation.setValue(geometry.topLeft());
    motion->scale.setValue(QPointF(geometry.width() / qreal(w->width()), geometry.height() / qreal(w->height())));
}

QRectF WindowMotionManager::targetGeometry(EffectWindow *w) const
{
    QHash<EffectWindow *, WindowMotion>::const_iterator it = m_managedWindows.constFind(w);
    if (it == m_managedWindows.end()) {
        return w->frameGeometry();
    }

    const WindowMotion *motion = &it.value();
    QRectF geometry(w->frameGeometry());

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    geometry.moveTo(motion->translation.target());
    geometry.setWidth(geometry.width() * motion->scale.target().x());
    geometry.setHeight(geometry.height() * motion->scale.target().y());

    return geometry;
}

EffectWindow *WindowMotionManager::windowAtPoint(QPoint point, bool useStackingOrder) const
{
    // TODO: Stacking order uses EffectsHandler::stackingOrder() then filters by m_managedWindows
    QHash<EffectWindow *, WindowMotion>::ConstIterator it = m_managedWindows.constBegin();
    while (it != m_managedWindows.constEnd()) {
        if (transformedGeometry(it.key()).contains(point)) {
            return it.key();
        }
        ++it;
    }

    return nullptr;
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
    QMatrix4x4 screenProjectionMatrix;
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
    : d(std::make_unique<EffectFramePrivate>())
{
}

EffectFrame::~EffectFrame() = default;

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

/***************************************************************
 TimeLine
***************************************************************/

class Q_DECL_HIDDEN TimeLine::Data : public QSharedData
{
public:
    std::chrono::milliseconds duration;
    Direction direction;
    QEasingCurve easingCurve;

    std::chrono::milliseconds elapsed = std::chrono::milliseconds::zero();
    std::optional<std::chrono::milliseconds> lastTimestamp = std::nullopt;
    bool done = false;
    RedirectMode sourceRedirectMode = RedirectMode::Relaxed;
    RedirectMode targetRedirectMode = RedirectMode::Strict;
};

TimeLine::TimeLine(std::chrono::milliseconds duration, Direction direction)
    : d(new Data)
{
    Q_ASSERT(duration > std::chrono::milliseconds::zero());
    d->duration = duration;
    d->direction = direction;
}

TimeLine::TimeLine(const TimeLine &other)
    : d(other.d)
{
}

TimeLine::~TimeLine() = default;

qreal TimeLine::progress() const
{
    return static_cast<qreal>(d->elapsed.count()) / d->duration.count();
}

qreal TimeLine::value() const
{
    const qreal t = progress();
    return d->easingCurve.valueForProgress(
        d->direction == Backward ? 1.0 - t : t);
}

void TimeLine::advance(std::chrono::milliseconds timestamp)
{
    if (d->done) {
        return;
    }

    std::chrono::milliseconds delta = std::chrono::milliseconds::zero();
    if (d->lastTimestamp.has_value()) {
        delta = timestamp - d->lastTimestamp.value();
    }

    Q_ASSERT(delta >= std::chrono::milliseconds::zero());
    d->lastTimestamp = timestamp;

    d->elapsed += delta;
    if (d->elapsed >= d->duration) {
        d->elapsed = d->duration;
        d->done = true;
        d->lastTimestamp = std::nullopt;
    }
}

std::chrono::milliseconds TimeLine::elapsed() const
{
    return d->elapsed;
}

void TimeLine::setElapsed(std::chrono::milliseconds elapsed)
{
    Q_ASSERT(elapsed >= std::chrono::milliseconds::zero());
    if (elapsed == d->elapsed) {
        return;
    }

    reset();

    d->elapsed = elapsed;

    if (d->elapsed >= d->duration) {
        d->elapsed = d->duration;
        d->done = true;
        d->lastTimestamp = std::nullopt;
    }
}

std::chrono::milliseconds TimeLine::duration() const
{
    return d->duration;
}

void TimeLine::setDuration(std::chrono::milliseconds duration)
{
    Q_ASSERT(duration > std::chrono::milliseconds::zero());
    if (duration == d->duration) {
        return;
    }
    d->elapsed = std::chrono::milliseconds(qRound(progress() * duration.count()));
    d->duration = duration;
    if (d->elapsed == d->duration) {
        d->done = true;
        d->lastTimestamp = std::nullopt;
    }
}

TimeLine::Direction TimeLine::direction() const
{
    return d->direction;
}

void TimeLine::setDirection(TimeLine::Direction direction)
{
    if (d->direction == direction) {
        return;
    }

    d->direction = direction;

    if (d->elapsed > std::chrono::milliseconds::zero()
        || d->sourceRedirectMode == RedirectMode::Strict) {
        d->elapsed = d->duration - d->elapsed;
    }

    if (d->done && d->targetRedirectMode == RedirectMode::Relaxed) {
        d->done = false;
    }

    if (d->elapsed >= d->duration) {
        d->done = true;
        d->lastTimestamp = std::nullopt;
    }
}

void TimeLine::toggleDirection()
{
    setDirection(d->direction == Forward ? Backward : Forward);
}

QEasingCurve TimeLine::easingCurve() const
{
    return d->easingCurve;
}

void TimeLine::setEasingCurve(const QEasingCurve &easingCurve)
{
    d->easingCurve = easingCurve;
}

void TimeLine::setEasingCurve(QEasingCurve::Type type)
{
    d->easingCurve.setType(type);
}

bool TimeLine::running() const
{
    return d->elapsed != std::chrono::milliseconds::zero()
        && d->elapsed != d->duration;
}

bool TimeLine::done() const
{
    return d->done;
}

void TimeLine::reset()
{
    d->lastTimestamp = std::nullopt;
    d->elapsed = std::chrono::milliseconds::zero();
    d->done = false;
}

TimeLine::RedirectMode TimeLine::sourceRedirectMode() const
{
    return d->sourceRedirectMode;
}

void TimeLine::setSourceRedirectMode(RedirectMode mode)
{
    d->sourceRedirectMode = mode;
}

TimeLine::RedirectMode TimeLine::targetRedirectMode() const
{
    return d->targetRedirectMode;
}

void TimeLine::setTargetRedirectMode(RedirectMode mode)
{
    d->targetRedirectMode = mode;
}

TimeLine &TimeLine::operator=(const TimeLine &other)
{
    d = other.d;
    return *this;
}

} // namespace

#include "moc_kwineffects.cpp"
#include "moc_kwinglobals.cpp"
