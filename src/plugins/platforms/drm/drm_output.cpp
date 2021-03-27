/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_output.h"
#include "drm_backend.h"
#include "drm_object_crtc.h"
#include "drm_object_connector.h"

#include "composite.h"
#include "cursor.h"
#include "logging.h"
#include "main.h"
#include "renderloop.h"
#include "screens.h"
#include "session.h"
// Qt
#include <QMatrix4x4>
#include <QCryptographicHash>
#include <QPainter>
// c++
#include <cerrno>
// drm
#include <xf86drm.h>
#include <libdrm/drm_mode.h>

#include "drm_gpu.h"
#include "drm_pipeline.h"

namespace KWin
{

DrmOutput::DrmOutput(DrmBackend *backend, DrmGpu *gpu)
    : AbstractWaylandOutput(backend)
    , m_backend(backend)
    , m_gpu(gpu)
    , m_renderLoop(new RenderLoop(this))
{
}

DrmOutput::~DrmOutput()
{
    Q_ASSERT(!m_pageFlipPending);
    teardown();
    delete m_pipeline;
}

RenderLoop *DrmOutput::renderLoop() const
{
    return m_renderLoop;
}

void DrmOutput::teardown()
{
    if (m_deleted) {
        return;
    }
    m_deleted = true;
    hideCursor();

    if (m_primaryPlane) {
        // TODO: when having multiple planes, also clean up these
        m_primaryPlane->setCurrent(nullptr);
    }

    if (!m_pageFlipPending) {
        deleteLater();
    } //else will be deleted in the page flip handler
    //this is needed so that the pageflipcallback handle isn't deleted
}

void DrmOutput::releaseBuffers()
{
    m_crtc->setCurrent(nullptr);
    m_crtc->setNext(nullptr);
    m_primaryPlane->setCurrent(nullptr);
    m_primaryPlane->setNext(nullptr);
}

bool DrmOutput::initCursor(const QSize &cursorSize)
{
    m_cursor = QSharedPointer<DrmDumbBuffer>::create(m_gpu, cursorSize);
    if (!m_cursor->map(QImage::Format_ARGB32_Premultiplied)) {
        return false;
    }
    return updateCursor();
}

bool DrmOutput::hideCursor()
{
    return m_pipeline->setCursor(nullptr);
}

bool DrmOutput::showCursor()
{
    return m_pipeline->setCursor(m_cursor);
}

static bool isCursorSpriteCompatible(const QImage *buffer, const QImage *sprite)
{
    // Note that we need compare the rects in the device independent pixels because the
    // buffer and the cursor sprite image may have different scale factors.
    const QRect bufferRect(QPoint(0, 0), buffer->size() / buffer->devicePixelRatio());
    const QRect spriteRect(QPoint(0, 0), sprite->size() / sprite->devicePixelRatio());

    return bufferRect.contains(spriteRect);
}

bool DrmOutput::updateCursor()
{
    if (m_deleted) {
        return false;
    }
    const Cursor *cursor = Cursors::self()->currentCursor();
    if (!cursor) {
        hideCursor();
        return true;
    }
    const QImage cursorImage = cursor->image();
    if (cursorImage.isNull()) {
        hideCursor();
        return true;
    }
    QImage *c = m_cursor->image();
    c->setDevicePixelRatio(scale());
    if (!isCursorSpriteCompatible(c, &cursorImage)) {
        // If the cursor image is too big, fall back to rendering the software cursor.
        return false;
    }
    c->fill(Qt::transparent);
    QPainter p;
    p.begin(c);
    p.setWorldTransform(logicalToNativeMatrix(cursor->rect(), 1, transform()).toTransform());
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();
    return m_pipeline->setCursor(m_cursor);
}

bool DrmOutput::moveCursor()
{
    Cursor *cursor = Cursors::self()->currentCursor();
    const QMatrix4x4 hotspotMatrix = logicalToNativeMatrix(cursor->rect(), scale(), transform());
    const QMatrix4x4 monitorMatrix = logicalToNativeMatrix(geometry(), scale(), transform());

    QPoint pos = monitorMatrix.map(cursor->pos());
    pos -= hotspotMatrix.map(cursor->hotspot());

    return m_pipeline->moveCursor(pos);
}

bool DrmOutput::init()
{
    if (m_gpu->atomicModeSetting() && !m_primaryPlane) {
        return false;
    }

    setSubPixelInternal(m_conn->subpixel());
    setInternal(m_conn->isInternal());
    setCapabilityInternal(Capability::Dpms);
    initOutputDevice();

    m_pipeline = new DrmPipeline(this, m_gpu, m_conn, m_crtc, m_primaryPlane, m_cursorPlane);
    updateMode(0);

    // renderloop will be un-inhibited when updating DPMS
    m_renderLoop->inhibit();
    m_dpmsEnabled = false;
    setDpmsMode(DpmsMode::On);
    return m_dpmsEnabled;
}

static bool checkIfEqual(_drmModeModeInfo one, _drmModeModeInfo two) {
    // for some reason a few values are 0
    // thus they're ignored here
    return one.clock       == two.clock
        && one.hdisplay    == two.hdisplay
        && one.hsync_start == two.hsync_start
        && one.hsync_end   == two.hsync_end
        && one.htotal      == two.htotal
        && one.hskew       == two.hskew
        && one.vdisplay    == two.vdisplay
        && one.vsync_start == two.vsync_start
        && one.vsync_end   == two.vsync_end
        && one.vtotal      == two.vtotal
        && one.vscan       == two.vscan
        && one.vrefresh    == two.vrefresh;
}

void DrmOutput::initOutputDevice()
{
    QVector<Mode> modes;
    auto modelist = m_conn->modes();
    auto currentMode = m_crtc->queryCurrentMode();
    for (int i = 0; i < modelist.count(); i++) {
        const auto &m = modelist[i];

        Mode mode;
        if (checkIfEqual(m.mode, currentMode)) {
            mode.flags |= ModeFlag::Current;
            m_conn->setModeIndex(i);
        }
        if (m.mode.type & DRM_MODE_TYPE_PREFERRED) {
            mode.flags |= ModeFlag::Preferred;
        }
        mode.id = i;
        mode.size = m.size;
        mode.refreshRate = m.refreshRate;
        modes << mode;
    }

    setName(m_conn->connectorName());
    initialize(m_conn->modelName(), m_conn->edid()->manufacturerString(),
               m_conn->edid()->eisaId(), m_conn->edid()->serialNumber(),
               m_conn->physicalSize(), modes, m_conn->edid()->raw());
}

void DrmOutput::updateEnablement(bool enable)
{
    if (m_dpmsEnabled == enable) {
        m_backend->enableOutput(this, enable);
        return;
    }

    if (m_pipeline->setEnablement(enable)) {
        m_backend->enableOutput(this, enable);
        if (enable) {
            m_renderLoop->inhibit();
        } else {
            m_renderLoop->uninhibit();
        }
        m_dpmsEnabled = enable;
    } else {
        qCWarning(KWIN_DRM) << "Setting enablement to" << enable << "failed!" << strerror(errno);
    }
}

void DrmOutput::setDpmsMode(DpmsMode mode)
{
    bool newDpmsEnable = mode == DpmsMode::On;
    if (newDpmsEnable == m_dpmsEnabled) {
        qCDebug(KWIN_DRM) << "New DPMS mode equals old mode. DPMS unchanged.";
        AbstractWaylandOutput::setDpmsModeInternal(mode);
        return;
    }
    if (m_pipeline->setEnablement(newDpmsEnable)) {
        m_dpmsEnabled = newDpmsEnable;
        AbstractWaylandOutput::setDpmsModeInternal(mode);
        if (m_dpmsEnabled) {
            m_renderLoop->uninhibit();
            if (Compositor *compositor = Compositor::self()) {
                compositor->addRepaintFull();
            }
        } else {
            m_renderLoop->inhibit();
        }
    } else {
        qCWarning(KWIN_DRM) << "Setting DPMS failed!" << strerror(errno);
    }
}

DrmPlane::Transformations outputToPlaneTransform(DrmOutput::Transform transform)
 {
    using OutTrans = DrmOutput::Transform;
    using PlaneTrans = DrmPlane::Transformation;

     // TODO: Do we want to support reflections (flips)?

     switch (transform) {
    case OutTrans::Normal:
    case OutTrans::Flipped:
        return PlaneTrans::Rotate0;
    case OutTrans::Rotated90:
    case OutTrans::Flipped90:
        return PlaneTrans::Rotate90;
    case OutTrans::Rotated180:
    case OutTrans::Flipped180:
        return PlaneTrans::Rotate180;
    case OutTrans::Rotated270:
    case OutTrans::Flipped270:
        return PlaneTrans::Rotate270;
     default:
         Q_UNREACHABLE();
     }
}

bool DrmOutput::hardwareTransforms() const
{
    if (!m_primaryPlane) {
        return false;
    }
    return m_primaryPlane->transformation() == outputToPlaneTransform(transform());
}

void DrmOutput::updateTransform(Transform transform)
{
    const auto planeTransform = outputToPlaneTransform(transform);

     if (m_primaryPlane) {
        // At the moment we have to exclude hardware transforms for vertical buffers.
        // For that we need to support other buffers and graceful fallback from atomic tests.
        // Reason is that standard linear buffers are not suitable.
        const bool isPortrait = transform == Transform::Rotated90
                                || transform == Transform::Flipped90
                                || transform == Transform::Rotated270
                                || transform == Transform::Flipped270;
        const auto &currentTransform = m_primaryPlane->transformation();
        if (!qEnvironmentVariableIsSet("KWIN_DRM_SW_ROTATIONS_ONLY") &&
                (m_primaryPlane->supportedTransformations() & planeTransform) &&
                !isPortrait) {
            m_primaryPlane->setTransformation(planeTransform);
        } else {
            m_primaryPlane->setTransformation(DrmPlane::Transformation::Rotate0);
        }
        if (!m_pipeline->test()) {
            m_primaryPlane->setTransformation(currentTransform);
        }
    }

    // show cursor only if is enabled, i.e if pointer device is presentP
    if (!m_backend->isCursorHidden() && !m_backend->usesSoftwareCursor()) {
        // the cursor might need to get rotated
        showCursor();
        updateCursor();
    }
}

void DrmOutput::updateMode(uint32_t width, uint32_t height, uint32_t refreshRate)
{
    if (m_conn->currentMode().size == QSize(width, height) && m_conn->currentMode().refreshRate == refreshRate) {
        return;
    }
    // try to find a fitting mode
    auto modelist = m_conn->modes();
    for (int i = 0; i < modelist.count(); i++) {
        if (modelist[i].size == QSize(width, height) && modelist[i].refreshRate == refreshRate) {
            updateMode(i);
            return;
        }
    }
    qCWarning(KWIN_DRM, "Could not find a fitting mode with size=%dx%d and refresh rate %d for output %s",
              width, height, refreshRate, qPrintable(name()));
}

void DrmOutput::updateMode(int modeIndex)
{
    const auto &modelist = m_conn->modes();
    if (modeIndex >= modelist.count()) {
        qCWarning(KWIN_DRM, "Mode %d for output %s doesn't exist", modeIndex, qPrintable(uuid().toString()));
        return;
    }
    const auto &mode = modelist[modeIndex];
    QSize srcSize = mode.size;
    if (hardwareTransforms() &&
        (transform() == Transform::Rotated270
        || transform() == Transform::Rotated90
        || transform() == Transform::Flipped90
        || transform() == Transform::Flipped270)) {
        srcSize = srcSize.transposed();
    }
    if (m_pipeline->modeset(srcSize, mode.mode)) {
        m_conn->setModeIndex(modeIndex);
        qCDebug(KWIN_DRM, "Modeset successful on %s (%dx%d@%f)", qPrintable(name()), modeSize().width(), modeSize().height(), refreshRate() * 0.001);
    } else {
        qCWarning(KWIN_DRM, "Modeset failed on %s (%dx%d@%f)! %s", qPrintable(name()), modeSize().width(), modeSize().height(), refreshRate() * 0.001, strerror(errno));
    }
    // this must be done even if it fails, for the initial modeset
    const auto &currentMode = m_conn->currentMode();
    AbstractWaylandOutput::setCurrentModeInternal(currentMode.size, currentMode.refreshRate);
    m_renderLoop->setRefreshRate(currentMode.refreshRate);
}

void DrmOutput::pageFlipped()
{
    m_pageFlipPending = false;
    if (m_deleted) {
        qCDebug(KWIN_DRM) << "delete-pageflip";
        deleteLater();
        return;
    }
    if (m_gpu->atomicModeSetting()) {
        m_primaryPlane->flipBuffer();
        if (m_cursorPlane) {
            m_cursorPlane->flipBuffer();
        }
    } else {
        m_crtc->flipBuffer();
    }
}

bool DrmOutput::present(const QSharedPointer<DrmBuffer> &buffer)
{
    if (!m_backend->session()->isActive()) {
        qCWarning(KWIN_DRM) << "session not active!";
        return false;
    }
    if (m_pageFlipPending) {
        qCWarning(KWIN_DRM) << "page not flipped yet!";
        return false;
    }
    if (m_pipeline->present(buffer)) {
        m_pageFlipPending = true;
        return true;
    } else {
        qCCritical(KWIN_DRM) << "present failed! This never should've happened!";
        return false;
    }
}

int DrmOutput::gammaRampSize() const
{
    return m_crtc->gammaRampSize();
}

bool DrmOutput::setGammaRamp(const GammaRamp &gamma)
{
    return m_pipeline->setGammaRamp(gamma);
}

}
