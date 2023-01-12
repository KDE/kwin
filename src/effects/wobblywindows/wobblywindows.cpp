/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wobblywindows.h"
#include "wobblywindowsconfig.h"

#include <cmath>

//#define COMPUTE_STATS

// if you enable it and run kwin in a terminal from the session it manages,
// be sure to redirect the output of kwin in a file or
// you'll propably get deadlocks.
//#define VERBOSE_MODE

#if defined COMPUTE_STATS && !defined VERBOSE_MODE
#ifdef __GNUC__
#warning "You enable COMPUTE_STATS without VERBOSE_MODE, computed stats will not be printed."
#endif
#endif

Q_LOGGING_CATEGORY(KWIN_WOBBLYWINDOWS, "kwin_effect_wobblywindows", QtWarningMsg)

namespace KWin
{

struct ParameterSet
{
    qreal stiffness;
    qreal drag;
    qreal move_factor;

    qreal xTesselation;
    qreal yTesselation;

    qreal minVelocity;
    qreal maxVelocity;
    qreal stopVelocity;
    qreal minAcceleration;
    qreal maxAcceleration;
    qreal stopAcceleration;
};

static const ParameterSet set_0 = {
    0.15,
    0.80,
    0.10,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

static const ParameterSet set_1 = {
    0.10,
    0.85,
    0.10,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

static const ParameterSet set_2 = {
    0.06,
    0.90,
    0.10,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

static const ParameterSet set_3 = {
    0.03,
    0.92,
    0.20,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

static const ParameterSet set_4 = {
    0.01,
    0.97,
    0.25,
    20.0,
    20.0,
    0.0,
    1000.0,
    0.5,
    0.0,
    1000.0,
    0.5,
};

static const ParameterSet pset[5] = {set_0, set_1, set_2, set_3, set_4};

WobblyWindowsEffect::WobblyWindowsEffect()
{
    initConfig<WobblyWindowsConfig>();
    reconfigure(ReconfigureAll);
    connect(effects, &EffectsHandler::windowStartUserMovedResized, this, &WobblyWindowsEffect::slotWindowStartUserMovedResized);
    connect(effects, &EffectsHandler::windowStepUserMovedResized, this, &WobblyWindowsEffect::slotWindowStepUserMovedResized);
    connect(effects, &EffectsHandler::windowFinishUserMovedResized, this, &WobblyWindowsEffect::slotWindowFinishUserMovedResized);
    connect(effects, &EffectsHandler::windowMaximizedStateChanged, this, &WobblyWindowsEffect::slotWindowMaximizeStateChanged);

    setVertexSnappingMode(RenderGeometry::VertexSnappingMode::None);
}

WobblyWindowsEffect::~WobblyWindowsEffect()
{
    if (!windows.empty()) {
        // we should be empty at this point...
        qCDebug(KWIN_WOBBLYWINDOWS) << "Windows list not empty. Left items : " << windows.count();
    }
}

void WobblyWindowsEffect::reconfigure(ReconfigureFlags)
{
    WobblyWindowsConfig::self()->read();

    QString settingsMode = WobblyWindowsConfig::settings();
    if (settingsMode != QStringLiteral("Custom")) {
        unsigned int wobblynessLevel = WobblyWindowsConfig::wobblynessLevel();
        if (wobblynessLevel > 4) {
            qCDebug(KWIN_WOBBLYWINDOWS) << "Wrong value for \"WobblynessLevel\" : " << wobblynessLevel;
            wobblynessLevel = 4;
        }
        setParameterSet(pset[wobblynessLevel]);

        if (WobblyWindowsConfig::advancedMode()) {
            m_stiffness = WobblyWindowsConfig::stiffness() / 100.0;
            m_drag = WobblyWindowsConfig::drag() / 100.0;
            m_move_factor = WobblyWindowsConfig::moveFactor() / 100.0;
        }
    } else { // Custom method, read all values from config file.
        m_stiffness = WobblyWindowsConfig::stiffness() / 100.0;
        m_drag = WobblyWindowsConfig::drag() / 100.0;
        m_move_factor = WobblyWindowsConfig::moveFactor() / 100.0;

        m_xTesselation = WobblyWindowsConfig::xTesselation();
        m_yTesselation = WobblyWindowsConfig::yTesselation();

        m_minVelocity = WobblyWindowsConfig::minVelocity();
        m_maxVelocity = WobblyWindowsConfig::maxVelocity();
        m_stopVelocity = WobblyWindowsConfig::stopVelocity();
        m_minAcceleration = WobblyWindowsConfig::minAcceleration();
        m_maxAcceleration = WobblyWindowsConfig::maxAcceleration();
        m_stopAcceleration = WobblyWindowsConfig::stopAcceleration();
    }

    m_moveWobble = WobblyWindowsConfig::moveWobble();
    m_resizeWobble = WobblyWindowsConfig::resizeWobble();

#if defined VERBOSE_MODE
    qCDebug(KWIN_WOBBLYWINDOWS) << "Parameters :\n"
                                << "grid(" << m_stiffness << ", " << m_drag << ", " << m_move_factor << ")\n"
                                << "velocity(" << m_minVelocity << ", " << m_maxVelocity << ", " << m_stopVelocity << ")\n"
                                << "acceleration(" << m_minAcceleration << ", " << m_maxAcceleration << ", " << m_stopAcceleration << ")\n"
                                << "tesselation(" << m_xTesselation << ", " << m_yTesselation << ")";
#endif
}

bool WobblyWindowsEffect::supported()
{
    return OffscreenEffect::supported() && effects->animationsSupported();
}

void WobblyWindowsEffect::setParameterSet(const ParameterSet &pset)
{
    m_stiffness = pset.stiffness;
    m_drag = pset.drag;
    m_move_factor = pset.move_factor;

    m_xTesselation = pset.xTesselation;
    m_yTesselation = pset.yTesselation;

    m_minVelocity = pset.minVelocity;
    m_maxVelocity = pset.maxVelocity;
    m_stopVelocity = pset.stopVelocity;
    m_minAcceleration = pset.minAcceleration;
    m_maxAcceleration = pset.maxAcceleration;
    m_stopAcceleration = pset.stopAcceleration;
}

void WobblyWindowsEffect::setVelocityThreshold(qreal m_minVelocity)
{
    this->m_minVelocity = m_minVelocity;
}

void WobblyWindowsEffect::setMoveFactor(qreal factor)
{
    m_move_factor = factor;
}

void WobblyWindowsEffect::setStiffness(qreal stiffness)
{
    m_stiffness = stiffness;
}

void WobblyWindowsEffect::setDrag(qreal drag)
{
    m_drag = drag;
}

void WobblyWindowsEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    // We need to mark the screen windows as transformed. Otherwise the whole
    // screen won't be repainted, resulting in artefacts.
    // Could we just set a subset of the screen to be repainted ?
    if (windows.count() != 0) {
        m_updateRegion = QRegion();
    }

    effects->prePaintScreen(data, presentTime);
}

static const std::chrono::milliseconds integrationStep(10);

void WobblyWindowsEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto infoIt = windows.find(w);
    if (infoIt != windows.end()) {
        data.setTransformed();

        // We have to reset the clip region in order to render clients below
        // opaque wobbly windows.
        data.opaque = QRegion();

        while ((presentTime - infoIt->clock).count() > 0) {
            const auto delta = std::min(presentTime - infoIt->clock, integrationStep);
            infoIt->clock += delta;

            if (!updateWindowWobblyDatas(w, delta.count())) {
                break;
            }
        }
    }

    effects->prePaintWindow(w, data, presentTime);
}

void WobblyWindowsEffect::apply(EffectWindow *w, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    if (!(mask & PAINT_SCREEN_TRANSFORMED) && windows.contains(w)) {
        quads = quads.makeRegularGrid(m_xTesselation, m_yTesselation);

        WindowWobblyInfos &wwi = windows[w];
        int tx = w->frameGeometry().x();
        int ty = w->frameGeometry().y();
        int width = w->frameGeometry().width();
        int height = w->frameGeometry().height();
        double left = 0.0;
        double top = 0.0;
        double right = w->width();
        double bottom = w->height();
        for (int i = 0; i < quads.count(); ++i) {
            for (int j = 0; j < 4; ++j) {
                WindowVertex &v = quads[i][j];
                Pair uv = {v.x() / width, v.y() / height};
                Pair newPos = computeBezierPoint(wwi, uv);
                v.move(newPos.x - tx, newPos.y - ty);
            }
            left = std::min(left, quads[i].left());
            top = std::min(top, quads[i].top());
            right = std::max(right, quads[i].right());
            bottom = std::max(bottom, quads[i].bottom());
        }
        QRectF dirtyRect(
            left * data.xScale() + w->x() + data.xTranslation(),
            top * data.yScale() + w->y() + data.yTranslation(),
            (right - left + 1.0) * data.xScale(),
            (bottom - top + 1.0) * data.yScale());
        // Expand the dirty region by 1px to fix potential round/floor issues.
        dirtyRect.adjust(-1.0, -1.0, 1.0, 1.0);
        m_updateRegion = m_updateRegion.united(dirtyRect.toRect());
    }
}

void WobblyWindowsEffect::postPaintScreen()
{
    if (!windows.isEmpty()) {
        effects->addRepaint(m_updateRegion);
    }

    // Call the next effect.
    effects->postPaintScreen();
}

void WobblyWindowsEffect::slotWindowStartUserMovedResized(EffectWindow *w)
{
    if (w->isSpecialWindow()) {
        return;
    }

    if ((w->isUserMove() && m_moveWobble) || (w->isUserResize() && m_resizeWobble)) {
        startMovedResized(w);
    }
}

void WobblyWindowsEffect::slotWindowStepUserMovedResized(EffectWindow *w, const QRectF &geometry)
{
    if (windows.contains(w)) {
        WindowWobblyInfos &wwi = windows[w];
        const QRectF rect = w->frameGeometry();
        if (rect.y() != wwi.resize_original_rect.y()) {
            wwi.can_wobble_top = true;
        }
        if (rect.x() != wwi.resize_original_rect.x()) {
            wwi.can_wobble_left = true;
        }
        if (rect.right() != wwi.resize_original_rect.right()) {
            wwi.can_wobble_right = true;
        }
        if (rect.bottom() != wwi.resize_original_rect.bottom()) {
            wwi.can_wobble_bottom = true;
        }
    }
}

void WobblyWindowsEffect::slotWindowFinishUserMovedResized(EffectWindow *w)
{
    if (windows.contains(w)) {
        WindowWobblyInfos &wwi = windows[w];
        wwi.status = Free;
        const QRectF rect = w->frameGeometry();
        if (rect.y() != wwi.resize_original_rect.y()) {
            wwi.can_wobble_top = true;
        }
        if (rect.x() != wwi.resize_original_rect.x()) {
            wwi.can_wobble_left = true;
        }
        if (rect.right() != wwi.resize_original_rect.right()) {
            wwi.can_wobble_right = true;
        }
        if (rect.bottom() != wwi.resize_original_rect.bottom()) {
            wwi.can_wobble_bottom = true;
        }
    }
}

void WobblyWindowsEffect::slotWindowMaximizeStateChanged(EffectWindow *w, bool horizontal, bool vertical)
{
    if (w->isUserMove() || w->isSpecialWindow()) {
        return;
    }

    if (m_moveWobble && m_resizeWobble) {
        stepMovedResized(w);
    }

    if (windows.contains(w)) {
        WindowWobblyInfos &wwi = windows[w];
        const QRectF rect = w->frameGeometry();
        if (rect.y() != wwi.resize_original_rect.y()) {
            wwi.can_wobble_top = true;
        }
        if (rect.x() != wwi.resize_original_rect.x()) {
            wwi.can_wobble_left = true;
        }
        if (rect.right() != wwi.resize_original_rect.right()) {
            wwi.can_wobble_right = true;
        }
        if (rect.bottom() != wwi.resize_original_rect.bottom()) {
            wwi.can_wobble_bottom = true;
        }
    }
}

void WobblyWindowsEffect::startMovedResized(EffectWindow *w)
{
    if (!windows.contains(w)) {
        WindowWobblyInfos new_wwi;
        initWobblyInfo(new_wwi, w->frameGeometry());
        windows[w] = new_wwi;
        redirect(w);
    }

    WindowWobblyInfos &wwi = windows[w];
    wwi.status = Moving;
    const QRectF &rect = w->frameGeometry();

    qreal x_increment = rect.width() / (wwi.width - 1.0);
    qreal y_increment = rect.height() / (wwi.height - 1.0);

    Pair picked = {static_cast<qreal>(cursorPos().x()), static_cast<qreal>(cursorPos().y())};
    int indx = (picked.x - rect.x()) / x_increment + 0.5;
    int indy = (picked.y - rect.y()) / y_increment + 0.5;
    int pickedPointIndex = indy * wwi.width + indx;
    if (pickedPointIndex < 0) {
        qCDebug(KWIN_WOBBLYWINDOWS) << "Picked index == " << pickedPointIndex << " with (" << cursorPos().x() << "," << cursorPos().y() << ")";
        pickedPointIndex = 0;
    } else if (static_cast<unsigned int>(pickedPointIndex) > wwi.count - 1) {
        qCDebug(KWIN_WOBBLYWINDOWS) << "Picked index == " << pickedPointIndex << " with (" << cursorPos().x() << "," << cursorPos().y() << ")";
        pickedPointIndex = wwi.count - 1;
    }
#if defined VERBOSE_MODE
    qCDebug(KWIN_WOBBLYWINDOWS) << "Original Picked point -- x : " << picked.x << " - y : " << picked.y;
#endif
    wwi.constraint[pickedPointIndex] = true;

    if (w->isUserResize()) {
        // on a resize, do not allow any edges to wobble until it has been moved from
        // its original location
        wwi.can_wobble_top = wwi.can_wobble_left = wwi.can_wobble_right = wwi.can_wobble_bottom = false;
        wwi.resize_original_rect = w->frameGeometry();
    } else {
        wwi.can_wobble_top = wwi.can_wobble_left = wwi.can_wobble_right = wwi.can_wobble_bottom = true;
    }
}

void WobblyWindowsEffect::stepMovedResized(EffectWindow *w)
{
    QRectF new_geometry = w->frameGeometry();
    if (!windows.contains(w)) {
        WindowWobblyInfos new_wwi;
        initWobblyInfo(new_wwi, new_geometry);
        windows[w] = new_wwi;
    }

    WindowWobblyInfos &wwi = windows[w];
    wwi.status = Free;

    QRectF maximized_area = effects->clientArea(MaximizeArea, w);
    bool throb_direction_out = (new_geometry.top() == maximized_area.top() && new_geometry.bottom() == maximized_area.bottom()) || (new_geometry.left() == maximized_area.left() && new_geometry.right() == maximized_area.right());
    qreal magnitude = throb_direction_out ? 10 : -30; // a small throb out when maximized, a larger throb inwards when restored
    for (unsigned int j = 0; j < wwi.height; ++j) {
        for (unsigned int i = 0; i < wwi.width; ++i) {
            Pair v = {magnitude * (i / qreal(wwi.width - 1) - 0.5), magnitude * (j / qreal(wwi.height - 1) - 0.5)};
            wwi.velocity[j * wwi.width + i] = v;
        }
    }

    // constrain the middle of the window, so that any asymetry wont cause it to drift off-center
    for (unsigned int j = 1; j < wwi.height - 1; ++j) {
        for (unsigned int i = 1; i < wwi.width - 1; ++i) {
            wwi.constraint[j * wwi.width + i] = true;
        }
    }
}

void WobblyWindowsEffect::initWobblyInfo(WindowWobblyInfos &wwi, QRectF geometry) const
{
    wwi.count = 4 * 4;
    wwi.width = 4;
    wwi.height = 4;

    wwi.bezierWidth = m_xTesselation;
    wwi.bezierHeight = m_yTesselation;
    wwi.bezierCount = m_xTesselation * m_yTesselation;

    wwi.origin.resize(wwi.count);
    wwi.position.resize(wwi.count);
    wwi.velocity.resize(wwi.count);
    wwi.acceleration.resize(wwi.count);
    wwi.buffer.resize(wwi.count);
    wwi.constraint.resize(wwi.count);

    wwi.bezierSurface.resize(wwi.bezierCount);

    wwi.status = Moving;
    wwi.clock = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());

    qreal x = geometry.x(), y = geometry.y();
    qreal width = geometry.width(), height = geometry.height();

    Pair initValue = {x, y};
    static const Pair nullPair = {0.0, 0.0};

    qreal x_increment = width / (wwi.width - 1.0);
    qreal y_increment = height / (wwi.height - 1.0);

    for (unsigned int j = 0; j < 4; ++j) {
        for (unsigned int i = 0; i < 4; ++i) {
            unsigned int idx = j * 4 + i;
            wwi.origin[idx] = initValue;
            wwi.position[idx] = initValue;
            wwi.velocity[idx] = nullPair;
            wwi.constraint[idx] = false;
            if (i != 4 - 2) { // x grid count - 2, i.e. not the last point
                initValue.x += x_increment;
            } else {
                initValue.x = width + x;
            }
            initValue.x = initValue.x;
        }
        initValue.x = x;
        initValue.x = initValue.x;
        if (j != 4 - 2) { // y grid count - 2, i.e. not the last point
            initValue.y += y_increment;
        } else {
            initValue.y = height + y;
        }
        initValue.y = initValue.y;
    }
}

WobblyWindowsEffect::Pair WobblyWindowsEffect::computeBezierPoint(const WindowWobblyInfos &wwi, Pair point) const
{
    const qreal tx = point.x;
    const qreal ty = point.y;

    // compute polynomial coeff

    qreal px[4];
    px[0] = (1 - tx) * (1 - tx) * (1 - tx);
    px[1] = 3 * (1 - tx) * (1 - tx) * tx;
    px[2] = 3 * (1 - tx) * tx * tx;
    px[3] = tx * tx * tx;

    qreal py[4];
    py[0] = (1 - ty) * (1 - ty) * (1 - ty);
    py[1] = 3 * (1 - ty) * (1 - ty) * ty;
    py[2] = 3 * (1 - ty) * ty * ty;
    py[3] = ty * ty * ty;

    Pair res = {0.0, 0.0};

    for (unsigned int j = 0; j < 4; ++j) {
        for (unsigned int i = 0; i < 4; ++i) {
            // this assume the grid is 4*4
            res.x += px[i] * py[j] * wwi.position[i + j * wwi.width].x;
            res.y += px[i] * py[j] * wwi.position[i + j * wwi.width].y;
        }
    }

    return res;
}

namespace
{

static inline void fixVectorBounds(WobblyWindowsEffect::Pair &vec, qreal min, qreal max)
{
    if (fabs(vec.x) < min) {
        vec.x = 0.0;
    } else if (fabs(vec.x) > max) {
        if (vec.x > 0.0) {
            vec.x = max;
        } else {
            vec.x = -max;
        }
    }

    if (fabs(vec.y) < min) {
        vec.y = 0.0;
    } else if (fabs(vec.y) > max) {
        if (vec.y > 0.0) {
            vec.y = max;
        } else {
            vec.y = -max;
        }
    }
}

#if defined COMPUTE_STATS
static inline void computeVectorBounds(WobblyWindowsEffect::Pair &vec, WobblyWindowsEffect::Pair &bound)
{
    if (fabs(vec.x) < bound.x) {
        bound.x = fabs(vec.x);
    } else if (fabs(vec.x) > bound.y) {
        bound.y = fabs(vec.x);
    }
    if (fabs(vec.y) < bound.x) {
        bound.x = fabs(vec.y);
    } else if (fabs(vec.y) > bound.y) {
        bound.y = fabs(vec.y);
    }
}
#endif

} // close the anonymous namespace

bool WobblyWindowsEffect::updateWindowWobblyDatas(EffectWindow *w, qreal time)
{
    QRectF rect = w->frameGeometry();
    WindowWobblyInfos &wwi = windows[w];

    qreal x_length = rect.width() / (wwi.width - 1.0);
    qreal y_length = rect.height() / (wwi.height - 1.0);

#if defined VERBOSE_MODE
    qCDebug(KWIN_WOBBLYWINDOWS) << "time " << time;
    qCDebug(KWIN_WOBBLYWINDOWS) << "increment x " << x_length << " // y" << y_length;
#endif

    Pair origine = {rect.x(), rect.y()};

    for (unsigned int j = 0; j < wwi.height; ++j) {
        for (unsigned int i = 0; i < wwi.width; ++i) {
            wwi.origin[wwi.width * j + i] = origine;
            if (i != wwi.width - 2) {
                origine.x += x_length;
            } else {
                origine.x = rect.width() + rect.x();
            }
        }
        origine.x = rect.x();
        if (j != wwi.height - 2) {
            origine.y += y_length;
        } else {
            origine.y = rect.height() + rect.y();
        }
    }

    Pair neibourgs[4];
    Pair acceleration;

    qreal acc_sum = 0.0;
    qreal vel_sum = 0.0;

    // compute acceleration, velocity and position for each point

    // for corners

    // top-left

    if (wwi.constraint[0]) {
        Pair window_pos = wwi.origin[0];
        Pair current_pos = wwi.position[0];
        Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
        Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
        wwi.acceleration[0] = accel;
    } else {
        Pair &pos = wwi.position[0];
        neibourgs[0] = wwi.position[1];
        neibourgs[1] = wwi.position[wwi.width];

        acceleration.x = ((neibourgs[0].x - pos.x) - x_length) * m_stiffness + (neibourgs[1].x - pos.x) * m_stiffness;
        acceleration.y = ((neibourgs[1].y - pos.y) - y_length) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness;

        acceleration.x /= 2;
        acceleration.y /= 2;

        wwi.acceleration[0] = acceleration;
    }

    // top-right

    if (wwi.constraint[wwi.width - 1]) {
        Pair window_pos = wwi.origin[wwi.width - 1];
        Pair current_pos = wwi.position[wwi.width - 1];
        Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
        Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
        wwi.acceleration[wwi.width - 1] = accel;
    } else {
        Pair &pos = wwi.position[wwi.width - 1];
        neibourgs[0] = wwi.position[wwi.width - 2];
        neibourgs[1] = wwi.position[2 * wwi.width - 1];

        acceleration.x = (x_length - (pos.x - neibourgs[0].x)) * m_stiffness + (neibourgs[1].x - pos.x) * m_stiffness;
        acceleration.y = ((neibourgs[1].y - pos.y) - y_length) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness;

        acceleration.x /= 2;
        acceleration.y /= 2;

        wwi.acceleration[wwi.width - 1] = acceleration;
    }

    // bottom-left

    if (wwi.constraint[wwi.width * (wwi.height - 1)]) {
        Pair window_pos = wwi.origin[wwi.width * (wwi.height - 1)];
        Pair current_pos = wwi.position[wwi.width * (wwi.height - 1)];
        Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
        Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
        wwi.acceleration[wwi.width * (wwi.height - 1)] = accel;
    } else {
        Pair &pos = wwi.position[wwi.width * (wwi.height - 1)];
        neibourgs[0] = wwi.position[wwi.width * (wwi.height - 1) + 1];
        neibourgs[1] = wwi.position[wwi.width * (wwi.height - 2)];

        acceleration.x = ((neibourgs[0].x - pos.x) - x_length) * m_stiffness + (neibourgs[1].x - pos.x) * m_stiffness;
        acceleration.y = (y_length - (pos.y - neibourgs[1].y)) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness;

        acceleration.x /= 2;
        acceleration.y /= 2;

        wwi.acceleration[wwi.width * (wwi.height - 1)] = acceleration;
    }

    // bottom-right

    if (wwi.constraint[wwi.count - 1]) {
        Pair window_pos = wwi.origin[wwi.count - 1];
        Pair current_pos = wwi.position[wwi.count - 1];
        Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
        Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
        wwi.acceleration[wwi.count - 1] = accel;
    } else {
        Pair &pos = wwi.position[wwi.count - 1];
        neibourgs[0] = wwi.position[wwi.count - 2];
        neibourgs[1] = wwi.position[wwi.width * (wwi.height - 1) - 1];

        acceleration.x = (x_length - (pos.x - neibourgs[0].x)) * m_stiffness + (neibourgs[1].x - pos.x) * m_stiffness;
        acceleration.y = (y_length - (pos.y - neibourgs[1].y)) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness;

        acceleration.x /= 2;
        acceleration.y /= 2;

        wwi.acceleration[wwi.count - 1] = acceleration;
    }

    // for borders

    // top border
    for (unsigned int i = 1; i < wwi.width - 1; ++i) {
        if (wwi.constraint[i]) {
            Pair window_pos = wwi.origin[i];
            Pair current_pos = wwi.position[i];
            Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
            Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
            wwi.acceleration[i] = accel;
        } else {
            Pair &pos = wwi.position[i];
            neibourgs[0] = wwi.position[i - 1];
            neibourgs[1] = wwi.position[i + 1];
            neibourgs[2] = wwi.position[i + wwi.width];

            acceleration.x = (x_length - (pos.x - neibourgs[0].x)) * m_stiffness + ((neibourgs[1].x - pos.x) - x_length) * m_stiffness + (neibourgs[2].x - pos.x) * m_stiffness;
            acceleration.y = ((neibourgs[2].y - pos.y) - y_length) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness + (neibourgs[1].y - pos.y) * m_stiffness;

            acceleration.x /= 3;
            acceleration.y /= 3;

            wwi.acceleration[i] = acceleration;
        }
    }

    // bottom border
    for (unsigned int i = wwi.width * (wwi.height - 1) + 1; i < wwi.count - 1; ++i) {
        if (wwi.constraint[i]) {
            Pair window_pos = wwi.origin[i];
            Pair current_pos = wwi.position[i];
            Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
            Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
            wwi.acceleration[i] = accel;
        } else {
            Pair &pos = wwi.position[i];
            neibourgs[0] = wwi.position[i - 1];
            neibourgs[1] = wwi.position[i + 1];
            neibourgs[2] = wwi.position[i - wwi.width];

            acceleration.x = (x_length - (pos.x - neibourgs[0].x)) * m_stiffness + ((neibourgs[1].x - pos.x) - x_length) * m_stiffness + (neibourgs[2].x - pos.x) * m_stiffness;
            acceleration.y = (y_length - (pos.y - neibourgs[2].y)) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness + (neibourgs[1].y - pos.y) * m_stiffness;

            acceleration.x /= 3;
            acceleration.y /= 3;

            wwi.acceleration[i] = acceleration;
        }
    }

    // left border
    for (unsigned int i = wwi.width; i < wwi.width * (wwi.height - 1); i += wwi.width) {
        if (wwi.constraint[i]) {
            Pair window_pos = wwi.origin[i];
            Pair current_pos = wwi.position[i];
            Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
            Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
            wwi.acceleration[i] = accel;
        } else {
            Pair &pos = wwi.position[i];
            neibourgs[0] = wwi.position[i + 1];
            neibourgs[1] = wwi.position[i - wwi.width];
            neibourgs[2] = wwi.position[i + wwi.width];

            acceleration.x = ((neibourgs[0].x - pos.x) - x_length) * m_stiffness + (neibourgs[1].x - pos.x) * m_stiffness + (neibourgs[2].x - pos.x) * m_stiffness;
            acceleration.y = (y_length - (pos.y - neibourgs[1].y)) * m_stiffness + ((neibourgs[2].y - pos.y) - y_length) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness;

            acceleration.x /= 3;
            acceleration.y /= 3;

            wwi.acceleration[i] = acceleration;
        }
    }

    // right border
    for (unsigned int i = 2 * wwi.width - 1; i < wwi.count - 1; i += wwi.width) {
        if (wwi.constraint[i]) {
            Pair window_pos = wwi.origin[i];
            Pair current_pos = wwi.position[i];
            Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
            Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
            wwi.acceleration[i] = accel;
        } else {
            Pair &pos = wwi.position[i];
            neibourgs[0] = wwi.position[i - 1];
            neibourgs[1] = wwi.position[i - wwi.width];
            neibourgs[2] = wwi.position[i + wwi.width];

            acceleration.x = (x_length - (pos.x - neibourgs[0].x)) * m_stiffness + (neibourgs[1].x - pos.x) * m_stiffness + (neibourgs[2].x - pos.x) * m_stiffness;
            acceleration.y = (y_length - (pos.y - neibourgs[1].y)) * m_stiffness + ((neibourgs[2].y - pos.y) - y_length) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness;

            acceleration.x /= 3;
            acceleration.y /= 3;

            wwi.acceleration[i] = acceleration;
        }
    }

    // for the inner points
    for (unsigned int j = 1; j < wwi.height - 1; ++j) {
        for (unsigned int i = 1; i < wwi.width - 1; ++i) {
            unsigned int index = i + j * wwi.width;

            if (wwi.constraint[index]) {
                Pair window_pos = wwi.origin[index];
                Pair current_pos = wwi.position[index];
                Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
                Pair accel = {move.x * m_stiffness, move.y * m_stiffness};
                wwi.acceleration[index] = accel;
            } else {
                Pair &pos = wwi.position[index];
                neibourgs[0] = wwi.position[index - 1];
                neibourgs[1] = wwi.position[index + 1];
                neibourgs[2] = wwi.position[index - wwi.width];
                neibourgs[3] = wwi.position[index + wwi.width];

                acceleration.x = ((neibourgs[0].x - pos.x) - x_length) * m_stiffness + (x_length - (pos.x - neibourgs[1].x)) * m_stiffness + (neibourgs[2].x - pos.x) * m_stiffness + (neibourgs[3].x - pos.x) * m_stiffness;
                acceleration.y = (y_length - (pos.y - neibourgs[2].y)) * m_stiffness + ((neibourgs[3].y - pos.y) - y_length) * m_stiffness + (neibourgs[0].y - pos.y) * m_stiffness + (neibourgs[1].y - pos.y) * m_stiffness;

                acceleration.x /= 4;
                acceleration.y /= 4;

                wwi.acceleration[index] = acceleration;
            }
        }
    }

    heightRingLinearMean(wwi.acceleration, wwi);

#if defined COMPUTE_STATS
    Pair accBound = {m_maxAcceleration, m_minAcceleration};
    Pair velBound = {m_maxVelocity, m_minVelocity};
#endif

    // compute the new velocity of each vertex.
    for (unsigned int i = 0; i < wwi.count; ++i) {
        Pair acc = wwi.acceleration[i];
        fixVectorBounds(acc, m_minAcceleration, m_maxAcceleration);

#if defined COMPUTE_STATS
        computeVectorBounds(acc, accBound);
#endif

        Pair &vel = wwi.velocity[i];
        vel.x = acc.x * time + vel.x * m_drag;
        vel.y = acc.y * time + vel.y * m_drag;

        acc_sum += fabs(acc.x) + fabs(acc.y);
    }

    heightRingLinearMean(wwi.velocity, wwi);

    // compute the new pos of each vertex.
    for (unsigned int i = 0; i < wwi.count; ++i) {
        Pair &pos = wwi.position[i];
        Pair &vel = wwi.velocity[i];

        fixVectorBounds(vel, m_minVelocity, m_maxVelocity);
#if defined COMPUTE_STATS
        computeVectorBounds(vel, velBound);
#endif

        pos.x += vel.x * time * m_move_factor;
        pos.y += vel.y * time * m_move_factor;

        vel_sum += fabs(vel.x) + fabs(vel.y);

#if defined VERBOSE_MODE
        if (wwi.constraint[i]) {
            qCDebug(KWIN_WOBBLYWINDOWS) << "Constraint point ** vel : " << vel.x << "," << vel.y << " ** move : " << vel.x * time << "," << vel.y * time;
        }
#endif
    }

    if (!wwi.can_wobble_top) {
        for (unsigned int i = 0; i < wwi.width; ++i) {
            for (unsigned j = 0; j < wwi.width - 1; ++j) {
                wwi.position[i + wwi.width * j].y = wwi.origin[i + wwi.width * j].y;
            }
        }
    }
    if (!wwi.can_wobble_bottom) {
        for (unsigned int i = wwi.width * (wwi.height - 1); i < wwi.count; ++i) {
            for (unsigned j = 0; j < wwi.width - 1; ++j) {
                wwi.position[i - wwi.width * j].y = wwi.origin[i - wwi.width * j].y;
            }
        }
    }
    if (!wwi.can_wobble_left) {
        for (unsigned int i = 0; i < wwi.count; i += wwi.width) {
            for (unsigned j = 0; j < wwi.width - 1; ++j) {
                wwi.position[i + j].x = wwi.origin[i + j].x;
            }
        }
    }
    if (!wwi.can_wobble_right) {
        for (unsigned int i = wwi.width - 1; i < wwi.count; i += wwi.width) {
            for (unsigned j = 0; j < wwi.width - 1; ++j) {
                wwi.position[i - j].x = wwi.origin[i - j].x;
            }
        }
    }

#if defined VERBOSE_MODE
#if defined COMPUTE_STATS
    qCDebug(KWIN_WOBBLYWINDOWS) << "Acceleration bounds (" << accBound.x << ", " << accBound.y << ")";
    qCDebug(KWIN_WOBBLYWINDOWS) << "Velocity bounds (" << velBound.x << ", " << velBound.y << ")";
#endif
    qCDebug(KWIN_WOBBLYWINDOWS) << "sum_acc : " << acc_sum << "  ***  sum_vel :" << vel_sum;
#endif

    if (wwi.status != Moving && acc_sum < m_stopAcceleration && vel_sum < m_stopVelocity) {
        windows.remove(w);
        unredirect(w);
        if (windows.isEmpty()) {
            effects->addRepaintFull();
        }
        return false;
    }

    return true;
}

void WobblyWindowsEffect::heightRingLinearMean(QVector<Pair> &data, WindowWobblyInfos &wwi)
{
    Pair neibourgs[8];

    // for corners

    // top-left
    {
        Pair &res = wwi.buffer[0];
        Pair vit = data[0];
        neibourgs[0] = data[1];
        neibourgs[1] = data[wwi.width];
        neibourgs[2] = data[wwi.width + 1];

        res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + 3.0 * vit.x) / 6.0;
        res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + 3.0 * vit.y) / 6.0;
    }

    // top-right
    {
        Pair &res = wwi.buffer[wwi.width - 1];
        Pair vit = data[wwi.width - 1];
        neibourgs[0] = data[wwi.width - 2];
        neibourgs[1] = data[2 * wwi.width - 1];
        neibourgs[2] = data[2 * wwi.width - 2];

        res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + 3.0 * vit.x) / 6.0;
        res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + 3.0 * vit.y) / 6.0;
    }

    // bottom-left
    {
        Pair &res = wwi.buffer[wwi.width * (wwi.height - 1)];
        Pair vit = data[wwi.width * (wwi.height - 1)];
        neibourgs[0] = data[wwi.width * (wwi.height - 1) + 1];
        neibourgs[1] = data[wwi.width * (wwi.height - 2)];
        neibourgs[2] = data[wwi.width * (wwi.height - 2) + 1];

        res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + 3.0 * vit.x) / 6.0;
        res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + 3.0 * vit.y) / 6.0;
    }

    // bottom-right
    {
        Pair &res = wwi.buffer[wwi.count - 1];
        Pair vit = data[wwi.count - 1];
        neibourgs[0] = data[wwi.count - 2];
        neibourgs[1] = data[wwi.width * (wwi.height - 1) - 1];
        neibourgs[2] = data[wwi.width * (wwi.height - 1) - 2];

        res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + 3.0 * vit.x) / 6.0;
        res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + 3.0 * vit.y) / 6.0;
    }

    // for borders

    // top border
    for (unsigned int i = 1; i < wwi.width - 1; ++i) {
        Pair &res = wwi.buffer[i];
        Pair vit = data[i];
        neibourgs[0] = data[i - 1];
        neibourgs[1] = data[i + 1];
        neibourgs[2] = data[i + wwi.width];
        neibourgs[3] = data[i + wwi.width - 1];
        neibourgs[4] = data[i + wwi.width + 1];

        res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + neibourgs[3].x + neibourgs[4].x + 5.0 * vit.x) / 10.0;
        res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + neibourgs[3].y + neibourgs[4].y + 5.0 * vit.y) / 10.0;
    }

    // bottom border
    for (unsigned int i = wwi.width * (wwi.height - 1) + 1; i < wwi.count - 1; ++i) {
        Pair &res = wwi.buffer[i];
        Pair vit = data[i];
        neibourgs[0] = data[i - 1];
        neibourgs[1] = data[i + 1];
        neibourgs[2] = data[i - wwi.width];
        neibourgs[3] = data[i - wwi.width - 1];
        neibourgs[4] = data[i - wwi.width + 1];

        res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + neibourgs[3].x + neibourgs[4].x + 5.0 * vit.x) / 10.0;
        res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + neibourgs[3].y + neibourgs[4].y + 5.0 * vit.y) / 10.0;
    }

    // left border
    for (unsigned int i = wwi.width; i < wwi.width * (wwi.height - 1); i += wwi.width) {
        Pair &res = wwi.buffer[i];
        Pair vit = data[i];
        neibourgs[0] = data[i + 1];
        neibourgs[1] = data[i - wwi.width];
        neibourgs[2] = data[i + wwi.width];
        neibourgs[3] = data[i - wwi.width + 1];
        neibourgs[4] = data[i + wwi.width + 1];

        res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + neibourgs[3].x + neibourgs[4].x + 5.0 * vit.x) / 10.0;
        res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + neibourgs[3].y + neibourgs[4].y + 5.0 * vit.y) / 10.0;
    }

    // right border
    for (unsigned int i = 2 * wwi.width - 1; i < wwi.count - 1; i += wwi.width) {
        Pair &res = wwi.buffer[i];
        Pair vit = data[i];
        neibourgs[0] = data[i - 1];
        neibourgs[1] = data[i - wwi.width];
        neibourgs[2] = data[i + wwi.width];
        neibourgs[3] = data[i - wwi.width - 1];
        neibourgs[4] = data[i + wwi.width - 1];

        res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + neibourgs[3].x + neibourgs[4].x + 5.0 * vit.x) / 10.0;
        res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + neibourgs[3].y + neibourgs[4].y + 5.0 * vit.y) / 10.0;
    }

    // for the inner points
    for (unsigned int j = 1; j < wwi.height - 1; ++j) {
        for (unsigned int i = 1; i < wwi.width - 1; ++i) {
            unsigned int index = i + j * wwi.width;

            Pair &res = wwi.buffer[index];
            Pair &vit = data[index];
            neibourgs[0] = data[index - 1];
            neibourgs[1] = data[index + 1];
            neibourgs[2] = data[index - wwi.width];
            neibourgs[3] = data[index + wwi.width];
            neibourgs[4] = data[index - wwi.width - 1];
            neibourgs[5] = data[index - wwi.width + 1];
            neibourgs[6] = data[index + wwi.width - 1];
            neibourgs[7] = data[index + wwi.width + 1];

            res.x = (neibourgs[0].x + neibourgs[1].x + neibourgs[2].x + neibourgs[3].x + neibourgs[4].x + neibourgs[5].x + neibourgs[6].x + neibourgs[7].x + 8.0 * vit.x) / 16.0;
            res.y = (neibourgs[0].y + neibourgs[1].y + neibourgs[2].y + neibourgs[3].y + neibourgs[4].y + neibourgs[5].y + neibourgs[6].y + neibourgs[7].y + 8.0 * vit.y) / 16.0;
        }
    }

    auto tmp = data;
    data = wwi.buffer;
    wwi.buffer = tmp;
}

bool WobblyWindowsEffect::isActive() const
{
    return !windows.isEmpty();
}

qreal WobblyWindowsEffect::stiffness() const
{
    return m_stiffness;
}

qreal WobblyWindowsEffect::drag() const
{
    return m_drag;
}

qreal WobblyWindowsEffect::moveFactor() const
{
    return m_move_factor;
}

qreal WobblyWindowsEffect::xTesselation() const
{
    return m_xTesselation;
}

qreal WobblyWindowsEffect::yTesselation() const
{
    return m_yTesselation;
}

qreal WobblyWindowsEffect::minVelocity() const
{
    return m_minVelocity;
}

qreal WobblyWindowsEffect::maxVelocity() const
{
    return m_maxVelocity;
}

qreal WobblyWindowsEffect::stopVelocity() const
{
    return m_stopVelocity;
}

qreal WobblyWindowsEffect::minAcceleration() const
{
    return m_minAcceleration;
}

qreal WobblyWindowsEffect::maxAcceleration() const
{
    return m_maxAcceleration;
}

qreal WobblyWindowsEffect::stopAcceleration() const
{
    return m_stopAcceleration;
}

bool WobblyWindowsEffect::isMoveWobble() const
{
    return m_moveWobble;
}

bool WobblyWindowsEffect::isResizeWobble() const
{
    return m_resizeWobble;
}

} // namespace KWin
