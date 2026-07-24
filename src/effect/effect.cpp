/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/effect.h"
#include "effect/effecthandler.h"

#include <KConfigGroup>

#include <QMatrix4x4>
#include <QVector3D>

namespace KWin
{

void WindowPrePaintData::setTranslucent()
{
    mask |= Effect::PAINT_WINDOW_TRANSLUCENT;
    mask &= ~Effect::PAINT_WINDOW_OPAQUE;
}

void WindowPrePaintData::setTransformed()
{
    mask |= Effect::PAINT_WINDOW_TRANSFORMED;
}

class PaintDataPrivate
{
public:
    PaintDataPrivate()
        : rotationAxis(0, 0, 1.)
        , rotationAngle(0.)
    {
    }
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
    qreal crossFadeProgress;
};

WindowPaintData::WindowPaintData()
    : PaintData()
    , d(std::make_unique<WindowPaintDataPrivate>())
{
    setOpacity(1.0);
    setSaturation(1.0);
    setBrightness(1.0);
    setCrossFadeProgress(0.0);
}

WindowPaintData::WindowPaintData(const WindowPaintData &other)
    : PaintData()
    , d(std::make_unique<WindowPaintDataPrivate>())
{
    translate(other.translation());
    setRotationOrigin(other.rotationOrigin());
    setRotationAxis(other.rotationAxis());
    setRotationAngle(other.rotationAngle());
    setOpacity(other.opacity());
    setSaturation(other.saturation());
    setBrightness(other.brightness());
    setCrossFadeProgress(other.crossFadeProgress());
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

void Effect::grabbedKeyboardEvent(QKeyEvent *)
{
}

bool Effect::borderActivated(ElectricBorder)
{
    return false;
}

void Effect::prePaintScreen(ScreenPrePaintData &data)
{
    effects->prePaintScreen(data);
}

bool Effect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
}

void Effect::postPaintScreen()
{
    effects->postPaintScreen();
}

void Effect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    effects->prePaintWindow(view, w, data);
}

bool Effect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    return effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
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

bool Effect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    return effects->drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

QPointF Effect::cursorPos()
{
    return effects->cursorPos();
}

std::chrono::milliseconds Effect::animationTime(const KConfigGroup &cfg, const QString &key, std::chrono::milliseconds defaultTime)
{
    if (const int time = cfg.readEntry(key, 0)) {
        return std::chrono::milliseconds(time);
    }

    const int actualTime = defaultTime.count() * effects->animationTimeFactor();
    return std::chrono::milliseconds(std::max(actualTime, 1));
}

std::chrono::milliseconds Effect::animationTime(std::chrono::milliseconds defaultTime)
{
    // at least 1ms, otherwise 0ms times can break some things
    const int actualTime = defaultTime.count() * effects->animationTimeFactor();
    return std::chrono::milliseconds(std::max(actualTime, 1));
}

int Effect::requestedEffectChainPosition() const
{
    return 0;
}

void Effect::pointerMotion(PointerMotionEvent *event)
{
}

void Effect::pointerButton(PointerButtonEvent *event)
{
}

void Effect::pointerAxis(PointerAxisEvent *event)
{
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

void Effect::touchCancel()
{
}

bool Effect::perform(Feature feature, const QVariantList &arguments)
{
    return false;
}

bool Effect::tabletToolProximity(TabletToolProximityEvent *event)
{
    return false;
}

bool Effect::tabletToolAxis(TabletToolAxisEvent *event)
{
    return false;
}

bool Effect::tabletToolTip(TabletToolTipEvent *event)
{
    return false;
}

bool Effect::tabletToolButtonEvent(uint button, bool pressed, quint64 toolId)
{
    return false;
}

bool Effect::tabletPadButtonEvent(uint button, bool pressed, void *device)
{
    return false;
}

bool Effect::tabletPadStripEvent(int number, qreal position, bool isFinger, void *device)
{
    return false;
}

bool Effect::tabletPadRingEvent(int number, qreal position, bool isFinger, void *device)
{
    return false;
}

bool Effect::tabletPadDialEvent(int number, double delta, void *device)
{
    return false;
}

bool Effect::blocksDirectScanout() const
{
    return true;
}

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

} // namespace KWin

#include "moc_effect.cpp"
