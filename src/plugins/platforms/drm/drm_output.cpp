/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "renderloop_p.h"
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

namespace KWin
{

DrmOutput::DrmOutput(DrmBackend *backend, DrmGpu *gpu)
    : AbstractWaylandOutput(backend)
    , m_backend(backend)
    , m_gpu(gpu)
    , m_renderLoop(new RenderLoop(this))
{
    m_turnOffTimer.setSingleShot(true);
    m_turnOffTimer.setInterval(dimAnimationTime());
    connect(&m_turnOffTimer, &QTimer::timeout, this, [this] {
        setDrmDpmsMode(DpmsMode::Off);
    });
}

DrmOutput::~DrmOutput()
{
    hideCursor();
    m_crtc->blank(this);

    if (m_primaryPlane) {
        // TODO: when having multiple planes, also clean up these
        m_primaryPlane->setCurrent(nullptr);
    }

    m_cursor[0].reset(nullptr);
    m_cursor[1].reset(nullptr);
    if (m_pageFlipPending) {
        pageFlipped();
    }
}

RenderLoop *DrmOutput::renderLoop() const
{
    return m_renderLoop;
}

bool DrmOutput::hideCursor()
{
    if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
        && isCursorVisible()) {
        m_renderLoop->scheduleRepaint();
    }
    return drmModeSetCursor(m_gpu->fd(), m_crtc->id(), 0, 0, 0) == 0;
}

bool DrmOutput::showCursor(DrmDumbBuffer *c)
{
    const QSize &s = c->size();
    if (drmModeSetCursor(m_gpu->fd(), m_crtc->id(), c->handle(), s.width(), s.height()) == 0) {
        if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
            && isCursorVisible()) {
            m_renderLoop->scheduleRepaint();
        }
        return true;
    } else {
        return false;
    }
}

bool DrmOutput::showCursor()
{
    const bool ret = showCursor(m_cursor[m_cursorIndex].data());
    if (!ret) {
        qCDebug(KWIN_DRM) << "DrmOutput::showCursor(DrmDumbBuffer) failed";
        return ret;
    }

    bool visibleBefore = isCursorVisible();
    if (m_hasNewCursor) {
        m_cursorIndex = (m_cursorIndex + 1) % 2;
        m_hasNewCursor = false;
    }
    if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
        && !visibleBefore
        && isCursorVisible()) {
        m_renderLoop->scheduleRepaint();
    }

    return ret;
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
    const Cursor *cursor = Cursors::self()->currentCursor();
    const QImage cursorImage = cursor->image();
    if (cursorImage.isNull()) {
        return false;
    }

    QImage *c = m_cursor[m_cursorIndex]->image();
    c->setDevicePixelRatio(scale());

    if (!isCursorSpriteCompatible(c, &cursorImage)) {
        // If the cursor image is too big, fall back to rendering the software cursor.
        return false;
    }

    m_hasNewCursor = true;
    c->fill(Qt::transparent);

    QPainter p;
    p.begin(c);
    p.setWorldTransform(logicalToNativeMatrix(cursor->rect(), 1, transform()).toTransform());
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();
    if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
        && isCursorVisible()) {
        m_renderLoop->scheduleRepaint();
    }
    return true;
}

void DrmOutput::moveCursor()
{
    Cursor *cursor = Cursors::self()->currentCursor();
    const QMatrix4x4 hotspotMatrix = logicalToNativeMatrix(cursor->rect(), scale(), transform());
    const QMatrix4x4 monitorMatrix = logicalToNativeMatrix(geometry(), scale(), transform());

    QPoint pos = monitorMatrix.map(cursor->pos());
    pos -= hotspotMatrix.map(cursor->hotspot());

    if (pos != m_cursorPos) {
        bool visible = isCursorVisible();
        drmModeMoveCursor(m_gpu->fd(), m_crtc->id(), pos.x(), pos.y());
        m_cursorPos = pos;
        if (RenderLoopPrivate::get(m_renderLoop)->presentMode == RenderLoopPrivate::SyncMode::Adaptive
            && (visible || visible != isCursorVisible())) {
            m_renderLoop->scheduleRepaint();
        }
    }
}

bool DrmOutput::init()
{
    if (m_gpu->atomicModeSetting() && !m_primaryPlane) {
        return false;
    }

    setSubPixelInternal(m_conn->subpixel());
    setInternal(m_conn->isInternal());
    setCapabilityInternal(DrmOutput::Capability::Dpms);
    if (m_conn->hasOverscan()) {
        setCapabilityInternal(Capability::Overscan);
        setOverscanInternal(m_conn->overscan());
    }
    if (m_conn->vrrCapable()) {
        setCapabilityInternal(Capability::Vrr);
        setVrrPolicy(RenderLoop::VrrPolicy::Automatic);
    }
    initOutputDevice();

    if (!m_gpu->atomicModeSetting() && !m_crtc->blank(this)) {
        // We use legacy mode and the initial output blank failed.
        return false;
    }

    setDpmsMode(DpmsMode::On);
    return true;
}

void DrmOutput::initOutputDevice()
{
    m_conn->findCurrentMode(m_crtc->queryCurrentMode());
    auto modelist = m_conn->modes();

    QVector<Mode> modes;
    modes.reserve(modelist.count());
    for (int i = 0; i < modelist.count(); ++i) {
        Mode mode;
        if (i == m_conn->currentModeIndex()) {
            mode.flags |= ModeFlag::Current;
        }
        if (modelist[i].mode.type & DRM_MODE_TYPE_PREFERRED) {
            mode.flags |= ModeFlag::Preferred;
        }

        mode.id = i;
        mode.size = modelist[i].size;
        mode.refreshRate = modelist[i].refreshRate;
        modes << mode;
    }

    setName(m_conn->connectorName());
    initialize(m_conn->modelName(), m_conn->edid()->manufacturerString(),
               m_conn->edid()->eisaId(), m_conn->edid()->serialNumber(),
               m_conn->physicalSize(), modes, m_conn->edid()->raw());
}

bool DrmOutput::initCursor(const QSize &cursorSize)
{
    auto createCursor = [this, cursorSize] (int index) {
        m_cursor[index].reset(new DrmDumbBuffer(m_gpu, cursorSize));
        if (!m_cursor[index]->map(QImage::Format_ARGB32_Premultiplied)) {
            return false;
        }
        return true;
    };
    if (!createCursor(0) || !createCursor(1)) {
        return false;
    }
    return true;
}

void DrmOutput::updateEnablement(bool enable)
{
    if (enable) {
        m_dpmsModePending = DpmsMode::On;
        if (m_gpu->atomicModeSetting()) {
            atomicEnable();
        } else {
            if (dpmsLegacyApply()) {
                m_backend->enableOutput(this, true);
            }
        }

    } else {
        m_dpmsModePending = DpmsMode::Off;
        if (m_gpu->atomicModeSetting()) {
            atomicDisable();
        } else {
            if (dpmsLegacyApply()) {
                m_backend->enableOutput(this, false);
            }
        }
    }
}

void DrmOutput::atomicEnable()
{
    m_modesetRequested = true;

    if (m_atomicOffPending) {
        Q_ASSERT(m_pageFlipPending);
        m_atomicOffPending = false;
    }
    m_backend->enableOutput(this, true);
    dpmsFinishOn();

    if (Compositor *compositor = Compositor::self()) {
        compositor->addRepaintFull();
    }
}

void DrmOutput::atomicDisable()
{
    m_modesetRequested = true;

    m_backend->enableOutput(this, false);
    m_atomicOffPending = true;
    if (!m_pageFlipPending) {
        dpmsAtomicOff();
    }
}

void DrmOutput::setDpmsMode(DpmsMode mode)
{
    if (mode == DpmsMode::Off) {
        if (!m_turnOffTimer.isActive()) {
            Q_EMIT aboutToTurnOff(std::chrono::milliseconds(m_turnOffTimer.interval()));
            m_turnOffTimer.start();
        }
        if (isEnabled()) {
            m_backend->createDpmsFilter();
        }
    } else {
        m_turnOffTimer.stop();
        setDrmDpmsMode(mode);
        Q_EMIT wakeUp();
    }
}

void DrmOutput::setDrmDpmsMode(DpmsMode mode)
{
    if (!m_conn->dpms() || !isEnabled()) {
        return;
    }

    if (mode == m_dpmsModePending) {
        return;
    }

    m_dpmsModePending = mode;

    if (m_gpu->atomicModeSetting()) {
        m_modesetRequested = true;
        if (mode == DpmsMode::On) {
            if (m_atomicOffPending) {
                Q_ASSERT(m_pageFlipPending);
                m_atomicOffPending = false;
            }
            dpmsFinishOn();
        } else {
            m_atomicOffPending = true;
            if (!m_pageFlipPending) {
                dpmsAtomicOff();
            }
        }
    } else {
       dpmsLegacyApply();
    }
}

void DrmOutput::dpmsFinishOn()
{
    qCDebug(KWIN_DRM) << "DPMS mode set for output" << m_crtc->id() << "to On.";

    m_backend->checkOutputsAreOn();
    m_crtc->blank(this);
    m_renderLoop->uninhibit();
    if (Compositor *compositor = Compositor::self()) {
        compositor->addRepaintFull();
    }
    if (!m_backend->isCursorHidden()) {
        showCursor();
    }
}

void DrmOutput::dpmsFinishOff()
{
    qCDebug(KWIN_DRM) << "DPMS mode set for output" << m_crtc->id() << "to Off.";

    if (isEnabled()) {
        m_backend->createDpmsFilter();
    }
    m_renderLoop->inhibit();
}

static uint64_t kwinDpmsModeToDrmDpmsMode(AbstractWaylandOutput::DpmsMode dpmsMode)
{
    switch (dpmsMode) {
    case AbstractWaylandOutput::DpmsMode::On:
        return DRM_MODE_DPMS_ON;
    case AbstractWaylandOutput::DpmsMode::Standby:
        return DRM_MODE_DPMS_STANDBY;
    case AbstractWaylandOutput::DpmsMode::Suspend:
        return DRM_MODE_DPMS_SUSPEND;
    case AbstractWaylandOutput::DpmsMode::Off:
        return DRM_MODE_DPMS_OFF;
    default:
        Q_UNREACHABLE();
    }
}

bool DrmOutput::dpmsLegacyApply()
{
    if (drmModeConnectorSetProperty(m_gpu->fd(), m_conn->id(),
                                    m_conn->dpms()->propId(),
                                    kwinDpmsModeToDrmDpmsMode(m_dpmsModePending)) < 0) {
        m_dpmsModePending = dpmsMode();
        qCWarning(KWIN_DRM) << "Setting DPMS failed";
        return false;
    }
    if (m_dpmsModePending == DpmsMode::On) {
        dpmsFinishOn();
    } else {
        dpmsFinishOff();
    }
    setDpmsModeInternal(m_dpmsModePending);
    return true;
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

        if (!qEnvironmentVariableIsSet("KWIN_DRM_SW_ROTATIONS_ONLY") &&
                (m_primaryPlane->supportedTransformations() & planeTransform) &&
                !isPortrait) {
            m_primaryPlane->setTransformation(planeTransform);
        } else {
            m_primaryPlane->setTransformation(DrmPlane::Transformation::Rotate0);
        }
    }
    m_modesetRequested = true;

    // show cursor only if is enabled, i.e if pointer device is presentP
    if (!m_backend->isCursorHidden() && !m_backend->usesSoftwareCursor()) {
        // the cursor might need to get rotated
        updateCursor();
        showCursor();
    }
}

void DrmOutput::updateMode(uint32_t width, uint32_t height, uint32_t refreshRate)
{
    if (m_conn->currentMode().size == QSize(width, height) && m_conn->currentMode().refreshRate == refreshRate) {
        return;
    }
    auto modelist = m_conn->modes();
    for (int i = 0; i < modelist.size(); i++) {
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
    if (m_conn->currentModeIndex() == modeIndex) {
        return;
    }
    m_conn->setModeIndex(modeIndex);
    m_modesetRequested = true;
    // aspect ratio might need to be adjusted
    m_conn->setOverscan(m_conn->overscan(), modeSize());
    setCurrentModeInternal();
}

void DrmOutput::setCurrentModeInternal()
{
    auto mode = m_conn->currentMode();
    AbstractWaylandOutput::setCurrentModeInternal(mode.size, mode.refreshRate);
}

void DrmOutput::pageFlipped()
{
    // In legacy mode we might get a page flip through a blank.
    Q_ASSERT(m_pageFlipPending || !m_gpu->atomicModeSetting());
    m_pageFlipPending = false;

    if (!m_crtc) {
        return;
    }
    if (m_gpu->atomicModeSetting()) {
        for (DrmPlane *p : qAsConst(m_nextPlanesFlipList)) {
            p->flipBuffer();
        }
        m_nextPlanesFlipList.clear();
    } else {
        m_crtc->flipBuffer();
    }

    if (m_atomicOffPending) {
        dpmsAtomicOff();
    }
}

bool DrmOutput::present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion)
{
    if (!buffer || buffer->bufferId() == 0) {
        return false;
    }
    if (m_dpmsModePending != DpmsMode::On) {
        return false;
    }
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    setVrr(renderLoopPrivate->presentMode == RenderLoopPrivate::SyncMode::Adaptive);
    if (m_gpu->atomicModeSetting() ? presentAtomically(buffer) : presentLegacy(buffer)) {
        Q_EMIT outputChange(damagedRegion);
        return true;
    } else {
        return false;
    }
}

bool DrmOutput::dpmsAtomicOff()
{
    m_atomicOffPending = false;

    // TODO: With multiple planes: deactivate all of them here
    hideCursor();
    m_primaryPlane->setNext(nullptr);
    m_nextPlanesFlipList << m_primaryPlane;

    if (!doAtomicCommit(AtomicCommitMode::Test)) {
        qCDebug(KWIN_DRM) << "Atomic test commit to Dpms Off failed. Aborting.";
        return false;
    }
    if (!doAtomicCommit(AtomicCommitMode::Real)) {
        qCDebug(KWIN_DRM) << "Atomic commit to Dpms Off failed. This should have never happened! Aborting.";
        return false;
    }
    m_nextPlanesFlipList.clear();
    dpmsFinishOff();

    return true;
}

bool DrmOutput::presentAtomically(const QSharedPointer<DrmBuffer> &buffer)
{
    if (!m_backend->session()->isActive()) {
        qCWarning(KWIN_DRM) << "Refusing to present output because session is inactive";
        return false;
    }

    if (m_pageFlipPending) {
        qCWarning(KWIN_DRM) << "Page not yet flipped.";
        return false;
    }

    // EglStreamBackend queues normal page flips through EGL when used as the rendering backend,
    // modesets are still performed through DRM-KMS
    if (m_gpu->useEglStreams() && !m_modesetRequested && m_gpu == m_backend->primaryGpu() && m_gpu->eglBackend() != nullptr) {
        m_pageFlipPending = true;
        return true;
    }

    m_primaryPlane->setNext(buffer);
    m_nextPlanesFlipList << m_primaryPlane;

    if (!doAtomicCommit(AtomicCommitMode::Test)) {
        //TODO: When we use planes for layered rendering, fallback to renderer instead. Also for direct scanout?
        //TODO: Probably should undo setNext and reset the flip list
        qCDebug(KWIN_DRM) << "Atomic test commit failed. Aborting present.";
        // go back to previous state
        if (m_lastWorkingState.valid) {
            m_conn->setModeIndex(m_lastWorkingState.modeIndex);
            setTransformInternal(m_lastWorkingState.transform);
            setGlobalPos(m_lastWorkingState.globalPos);
            if (m_primaryPlane) {
                m_primaryPlane->setTransformation(m_lastWorkingState.planeTransformations);
            }
            m_modesetRequested = true;
            if (!m_backend->isCursorHidden()) {
                // the cursor might need to get rotated
                updateCursor();
                showCursor();
            }
            // aspect ratio might need to be adjusted
            m_conn->setOverscan(m_conn->overscan(), modeSize());
            setCurrentModeInternal();
            Q_EMIT screens()->changed();
        }
        return false;
    }
    const bool wasModeset = m_modesetRequested;
    if (!doAtomicCommit(AtomicCommitMode::Real)) {
        qCDebug(KWIN_DRM) << "Atomic commit failed. This should have never happened! Aborting present.";
        //TODO: Probably should undo setNext and reset the flip list
        return false;
    }
    if (wasModeset) {
        // store current mode set as new good state
        m_lastWorkingState.modeIndex = m_conn->currentModeIndex();
        m_lastWorkingState.transform = transform();
        m_lastWorkingState.globalPos = globalPos();
        if (m_primaryPlane) {
            m_lastWorkingState.planeTransformations = m_primaryPlane->transformation();
        }
        m_lastWorkingState.valid = true;
        m_renderLoop->setRefreshRate(m_conn->currentMode().refreshRate);
    }
    m_pageFlipPending = true;
    return true;
}

bool DrmOutput::presentLegacy(const QSharedPointer<DrmBuffer> &buffer)
{
    if (m_crtc->next()) {
        return false;
    }
    if (!m_backend->session()->isActive()) {
        m_crtc->setNext(buffer);
        return false;
    }

    // Do we need to set a new mode first?
    if (!m_crtc->current() || m_crtc->current()->needsModeChange(buffer.get())) {
        if (!setModeLegacy(buffer.get())) {
            return false;
        }
    }
    const bool ok = drmModePageFlip(m_gpu->fd(), m_crtc->id(), buffer->bufferId(), DRM_MODE_PAGE_FLIP_EVENT, this) == 0;
    if (ok) {
        m_crtc->setNext(buffer);
        m_pageFlipPending = true;
    } else {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno);
    }
    return ok;
}

bool DrmOutput::setModeLegacy(DrmBuffer *buffer)
{
    uint32_t connId = m_conn->id();
    auto mode = m_conn->currentMode().mode;
    if (drmModeSetCrtc(m_gpu->fd(), m_crtc->id(), buffer->bufferId(), 0, 0, &connId, 1, &mode) == 0) {
        return true;
    } else {
        qCWarning(KWIN_DRM) << "Mode setting failed";
        return false;
    }
}

bool DrmOutput::doAtomicCommit(AtomicCommitMode mode)
{
    drmModeAtomicReq *req = drmModeAtomicAlloc();

    auto errorHandler = [this, mode, req] () {
        if (mode == AtomicCommitMode::Test) {
            // TODO: when we later test overlay planes, make sure we change only the right stuff back
        }
        if (req) {
            drmModeAtomicFree(req);
        }

        if (dpmsMode() != m_dpmsModePending) {
            qCWarning(KWIN_DRM) << "Setting DPMS failed";
            m_dpmsModePending = dpmsMode();
            if (dpmsMode() != DpmsMode::On) {
                dpmsFinishOff();
            }
        }

        // TODO: see above, rework later for overlay planes!
        for (DrmPlane *p : qAsConst(m_nextPlanesFlipList)) {
            p->setNext(nullptr);
        }
        m_nextPlanesFlipList.clear();

    };

    if (!req) {
        qCWarning(KWIN_DRM) << "DRM: couldn't allocate atomic request";
        errorHandler();
        return false;
    }

    uint32_t flags = 0;
    // Do we need to set a new mode?
    if (m_modesetRequested) {
        if (m_dpmsModePending == DpmsMode::On) {
            auto mode = m_conn->currentMode();
            if (drmModeCreatePropertyBlob(m_gpu->fd(), &mode, sizeof(mode), &m_blobId) != 0) {
                qCWarning(KWIN_DRM) << "Failed to create property blob";
                errorHandler();
                return false;
            }
        }
        setModesetValues(m_dpmsModePending == DpmsMode::On);
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    }

    if (!m_conn->atomicPopulate(req)) {
        qCWarning(KWIN_DRM) << "Failed to populate connector. Abort atomic commit!";
        errorHandler();
        return false;
    }

    if (!m_crtc->atomicPopulate(req)) {
        qCWarning(KWIN_DRM) << "Failed to populate crtc. Abort atomic commit!";
        errorHandler();
        return false;
    }

    if (mode == AtomicCommitMode::Real) {
        if (m_dpmsModePending == DpmsMode::On) {
            if (!(flags & DRM_MODE_ATOMIC_ALLOW_MODESET)) {
                // TODO: Evaluating this condition should only be necessary, as long as we expect older kernels than 4.10.
                flags |= DRM_MODE_ATOMIC_NONBLOCK;
            }

            // EglStreamBackend uses the NV_output_drm_flip_event EGL extension
            // to register the flip event through eglStreamConsumerAcquireAttribNV
            // but only when used as the rendering backend
            if (!m_gpu->useEglStreams() || m_gpu != m_backend->primaryGpu() || m_gpu->eglBackend() == nullptr) {
                flags |= DRM_MODE_PAGE_FLIP_EVENT;
            }
        }
    } else {
        flags |= DRM_MODE_ATOMIC_TEST_ONLY;
    }

    bool ret = true;
    // TODO: Make sure when we use more than one plane at a time, that we go through this list in the right order.
    for (int i = m_nextPlanesFlipList.size() - 1; 0 <= i; i-- ) {
        DrmPlane *p = m_nextPlanesFlipList[i];
        ret &= p->atomicPopulate(req);
    }

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic planes. Abort atomic commit!";
        errorHandler();
        return false;
    }

    if (drmModeAtomicCommit(m_gpu->fd(), req, flags, this)) {
        qCDebug(KWIN_DRM) << "Atomic request failed to commit: " << strerror(errno);
        errorHandler();
        return false;
    }

    if (mode == AtomicCommitMode::Real && (flags & DRM_MODE_ATOMIC_ALLOW_MODESET)) {
        qCDebug(KWIN_DRM) << "Atomic Modeset successful.";
        m_modesetRequested = false;
        setDpmsModeInternal(m_dpmsModePending);
    }

    drmModeAtomicFree(req);
    return true;
}

void DrmOutput::setModesetValues(bool enable)
{
    if (enable) {
        const QSize mSize = modeSize();
        const QSize bufferSize = m_primaryPlane->next() ? m_primaryPlane->next()->size() : pixelSize();
        const QSize sourceSize = hardwareTransforms() ? bufferSize : mSize;
        QRect targetRect = QRect(QPoint(0, 0), mSize);
        if (mSize != sourceSize) {
            targetRect.setSize(sourceSize.scaled(mSize, Qt::AspectRatioMode::KeepAspectRatio));
            targetRect.setX((mSize.width() - targetRect.width()) / 2);
            targetRect.setY((mSize.height() - targetRect.height()) / 2);
        }

        m_primaryPlane->setValue(DrmPlane::PropertyIndex::SrcX, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::SrcY, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::SrcW, sourceSize.width() << 16);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::SrcH, sourceSize.height() << 16);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcX, targetRect.x());
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcY, targetRect.y());
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcW, targetRect.width());
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcH, targetRect.height());
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcId, m_crtc->id());
    } else {
        m_primaryPlane->setCurrent(nullptr);
        m_primaryPlane->setNext(nullptr);

        m_primaryPlane->setValue(DrmPlane::PropertyIndex::SrcX, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::SrcY, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::SrcW, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::SrcH, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcX, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcY, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcW, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcH, 0);
        m_primaryPlane->setValue(DrmPlane::PropertyIndex::CrtcId, 0);
    }
    m_conn->setValue(DrmConnector::PropertyIndex::CrtcId, enable ? m_crtc->id() : 0);
    m_crtc->setValue(DrmCrtc::PropertyIndex::ModeId, enable ? m_blobId : 0);
    m_crtc->setValue(DrmCrtc::PropertyIndex::Active, enable);
}

int DrmOutput::gammaRampSize() const
{
    return m_crtc->gammaRampSize();
}

bool DrmOutput::setGammaRamp(const GammaRamp &gamma)
{
    return m_crtc->setGammaRamp(gamma);
}

void DrmOutput::setOverscan(uint32_t overscan)
{
    if (m_conn->hasOverscan() && overscan <= 100) {
        m_conn->setOverscan(overscan, modeSize());
        setOverscanInternal(m_conn->overscan());
    }
}

void DrmOutput::setVrr(bool enable)
{
    if (!m_conn->vrrCapable() || m_crtc->isVrrEnabled() == enable) {
        return;
    }
    if (!m_crtc->setVrr(enable) || (m_gpu->atomicModeSetting() && !doAtomicCommit(AtomicCommitMode::Test))) {
        qCWarning(KWIN_DRM) << "Failed to set vrr on" << this;
        setVrrPolicy(RenderLoop::VrrPolicy::Never);
        m_crtc->setVrr(false);
    }
}

bool DrmOutput::isCursorVisible()
{
    return m_cursor[m_cursorIndex] && QRect(m_cursorPos, m_cursor[m_cursorIndex]->size()).intersects(QRect(0, 0, modeSize().width(), modeSize().height()));
}

DrmBuffer *DrmOutput::currentBuffer() const
{
    if (m_primaryPlane) {
        return m_primaryPlane->current().get();
    } else {
        return m_crtc->current().get();
    }
}

}
