/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_pipeline.h"

#include <errno.h>

#include "logging.h"
#include "drm_gpu.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "drm_buffer.h"
#include "cursor.h"
#include "session.h"
#include "drm_output.h"
#include "drm_backend.h"
#include "egl_gbm_backend.h"
#include "drm_buffer_gbm.h"

#include <gbm.h>
#include <drm_fourcc.h>

namespace KWin
{

DrmPipeline::DrmPipeline(DrmConnector *conn)
    : m_output(nullptr)
    , m_connector(conn)
{
}

DrmPipeline::~DrmPipeline()
{
    m_output = nullptr;
    if (m_pageflipPending && m_current.crtc) {
        pageFlipped({});
    }
}

bool DrmPipeline::present(const QSharedPointer<DrmBuffer> &buffer)
{
    Q_ASSERT(pending.crtc);
    Q_ASSERT(buffer);
    m_primaryBuffer = buffer;
    auto buf = dynamic_cast<DrmGbmBuffer*>(buffer.data());
    // with direct scanout disallow modesets, calling presentFailed() and logging warnings
    bool directScanout = buf && buf->clientBuffer();
    if (gpu()->needsModeset()) {
        if (directScanout) {
            return false;
        }
        m_modesetPresentPending = true;
        return gpu()->maybeModeset();
    }
    if (gpu()->atomicModeSetting()) {
        if (!commitPipelines({this}, CommitMode::Commit)) {
            // update properties and try again
            m_connector->updateProperties();
            if (pending.crtc) {
                pending.crtc->updateProperties();
                if (pending.crtc->primaryPlane()) {
                    pending.crtc->primaryPlane()->updateProperties();
                }
                if (pending.crtc->cursorPlane()) {
                    pending.crtc->cursorPlane()->updateProperties();
                }
            }
            if (!commitPipelines({this}, CommitMode::Commit)) {
                if (directScanout) {
                    return false;
                }
                qCWarning(KWIN_DRM) << "Atomic present failed!" << strerror(errno);
                printDebugInfo();
                if (m_output) {
                    m_output->presentFailed();
                }
                return false;
            }
        }
    } else {
        if (!presentLegacy()) {
            qCWarning(KWIN_DRM) << "Present failed!" << strerror(errno);
            if (m_output) {
                m_output->presentFailed();
            }
            return false;
        }
    }
    return true;
}

bool DrmPipeline::commitPipelines(const QVector<DrmPipeline*> &pipelines, CommitMode mode, const QVector<DrmObject*> &unusedObjects)
{
    Q_ASSERT(!pipelines.isEmpty());
    if (pipelines[0]->gpu()->atomicModeSetting()) {
        return commitPipelinesAtomic(pipelines, mode, unusedObjects);
    } else {
        return commitPipelinesLegacy(pipelines, mode);
    }
}

bool DrmPipeline::commitPipelinesAtomic(const QVector<DrmPipeline*> &pipelines, CommitMode mode, const QVector<DrmObject*> &unusedObjects)
{
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    if (!req) {
        qCCritical(KWIN_DRM) << "Failed to allocate drmModeAtomicReq!" << strerror(errno);
        return false;
    }
    uint32_t flags = 0;
    const auto &failed = [pipelines, req, &flags, unusedObjects](){
        drmModeAtomicFree(req);
        printFlags(flags);
        for (const auto &pipeline : pipelines) {
            pipeline->printDebugInfo();
            pipeline->atomicCommitFailed();
        }
        for (const auto &obj : unusedObjects) {
            printProps(obj, PrintMode::OnlyChanged);
            obj->rollbackPending();
        }
        return false;
    };
    for (const auto &pipeline : pipelines) {
        if (!pipeline->checkTestBuffer()) {
            qCWarning(KWIN_DRM) << "Checking test buffer failed for" << mode;
            return failed();
        }
        if (!pipeline->populateAtomicValues(req, flags)) {
            qCWarning(KWIN_DRM) << "Populating atomic values failed for" << mode;
            return failed();
        }
    }
    for (const auto &unused : unusedObjects) {
        unused->disable();
        if (unused->needsModeset()) {
            flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
        }
        if (!unused->atomicPopulate(req)) {
            qCWarning(KWIN_DRM) << "Populating atomic values failed for unused resource" << unused;
            return failed();
        }
    }
    bool modeset = flags & DRM_MODE_ATOMIC_ALLOW_MODESET;
    Q_ASSERT(!modeset || mode != CommitMode::Commit);
    if (modeset) {
        // The kernel fails commits with DRM_MODE_PAGE_FLIP_EVENT when a crtc is disabled in the commit
        // and already was disabled before, to work around some quirks in old userspace.
        // Instead of using DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, do the modeset in a blocking
        // fashion without page flip events and directly call the pageFlipped method afterwards
        flags = flags & (~DRM_MODE_PAGE_FLIP_EVENT);
    } else {
        flags |= DRM_MODE_ATOMIC_NONBLOCK;
    }
    if (drmModeAtomicCommit(pipelines[0]->gpu()->fd(), req, (flags & (~DRM_MODE_PAGE_FLIP_EVENT)) | DRM_MODE_ATOMIC_TEST_ONLY, nullptr) != 0) {
        qCDebug(KWIN_DRM) << "Atomic test for" << mode << "failed!" << strerror(errno);
        return failed();
    }
    if (mode != CommitMode::Test && drmModeAtomicCommit(pipelines[0]->gpu()->fd(), req, flags, nullptr) != 0) {
        qCCritical(KWIN_DRM) << "Atomic commit failed! This should never happen!" << strerror(errno);
        return failed();
    }
    for (const auto &pipeline : pipelines) {
        pipeline->atomicCommitSuccessful(mode);
    }
    for (const auto &obj : unusedObjects) {
        obj->commitPending();
        if (mode != CommitMode::Test) {
            obj->commit();
        }
    }
    drmModeAtomicFree(req);
    return true;
}

bool DrmPipeline::populateAtomicValues(drmModeAtomicReq *req, uint32_t &flags)
{
    if (needsModeset()) {
        prepareAtomicModeset();
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    }
    if (activePending()) {
        flags |= DRM_MODE_PAGE_FLIP_EVENT;
    }
    if (pending.crtc) {
        pending.crtc->setPending(DrmCrtc::PropertyIndex::VrrEnabled, pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive);
        pending.crtc->setPending(DrmCrtc::PropertyIndex::Gamma_LUT, pending.gamma ? pending.gamma->blobId() : 0);
        auto modeSize = m_connector->modes().at(pending.modeIndex)->size();
        pending.crtc->primaryPlane()->set(QPoint(0, 0), m_primaryBuffer ? m_primaryBuffer->size() : bufferSize(), QPoint(0, 0), modeSize);
        pending.crtc->primaryPlane()->setBuffer(activePending() ? m_primaryBuffer.get() : nullptr);

        if (pending.crtc->cursorPlane()) {
            pending.crtc->cursorPlane()->set(QPoint(0, 0), gpu()->cursorSize(), pending.cursorPos, gpu()->cursorSize());
            pending.crtc->cursorPlane()->setBuffer(activePending() ? pending.cursorBo.get() : nullptr);
            pending.crtc->cursorPlane()->setPending(DrmPlane::PropertyIndex::CrtcId, (activePending() && pending.cursorBo) ? pending.crtc->id() : 0);
        }
    }
    if (!m_connector->atomicPopulate(req)) {
        return false;
    }
    if (pending.crtc) {
        if (!pending.crtc->atomicPopulate(req)) {
            return false;
        }
        if (!pending.crtc->primaryPlane()->atomicPopulate(req)) {
            return false;
        }
        if (pending.crtc->cursorPlane() && !pending.crtc->cursorPlane()->atomicPopulate(req)) {
            return false;
        }
    }
    return true;
}

void DrmPipeline::prepareAtomicModeset()
{
    if (!pending.crtc) {
        m_connector->setPending(DrmConnector::PropertyIndex::CrtcId, 0);
        return;
    }
    auto mode = m_connector->modes().at(pending.modeIndex);

    m_connector->setPending(DrmConnector::PropertyIndex::CrtcId, activePending() ? pending.crtc->id() : 0);
    if (const auto &prop = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
        prop->setEnum(pending.rgbRange);
    }
    if (const auto &prop = m_connector->getProp(DrmConnector::PropertyIndex::LinkStatus)) {
        prop->setEnum(DrmConnector::LinkStatus::Good);
    }
    if (const auto overscan = m_connector->getProp(DrmConnector::PropertyIndex::Overscan)) {
        overscan->setPending(pending.overscan);
    } else if (const auto underscan = m_connector->getProp(DrmConnector::PropertyIndex::Underscan)) {
        const uint32_t hborder = calculateUnderscan();
        underscan->setEnum(pending.overscan != 0 ? DrmConnector::UnderscanOptions::On : DrmConnector::UnderscanOptions::Off);
        m_connector->getProp(DrmConnector::PropertyIndex::Underscan_vborder)->setPending(pending.overscan);
        m_connector->getProp(DrmConnector::PropertyIndex::Underscan_hborder)->setPending(hborder);
    }

    pending.crtc->setPending(DrmCrtc::PropertyIndex::Active, activePending());
    pending.crtc->setPending(DrmCrtc::PropertyIndex::ModeId, activePending() ? mode->blobId() : 0);

    pending.crtc->primaryPlane()->setPending(DrmPlane::PropertyIndex::CrtcId, activePending() ? pending.crtc->id() : 0);
    pending.crtc->primaryPlane()->setTransformation(pending.bufferTransformation);
    if (pending.crtc->cursorPlane()) {
        pending.crtc->cursorPlane()->setTransformation(DrmPlane::Transformation::Rotate0);
    }
}

uint32_t DrmPipeline::calculateUnderscan()
{
    const auto modes = m_connector->modes();
    const auto size = modes[pending.modeIndex]->size();
    const float aspectRatio = size.width() / static_cast<float>(size.height());
    uint32_t hborder = pending.overscan * aspectRatio;
    if (hborder > 128) {
        // overscan only goes from 0-100 so we cut off the 101-128 value range of underscan_vborder
        hborder = 128;
        pending.overscan = 128 / aspectRatio;
    }
    return hborder;
}

void DrmPipeline::atomicCommitFailed()
{
    if (m_oldTestBuffer) {
        m_primaryBuffer = m_oldTestBuffer;
        m_oldTestBuffer = nullptr;
    }
    m_connector->rollbackPending();
    if (pending.crtc) {
        pending.crtc->rollbackPending();
        pending.crtc->primaryPlane()->rollbackPending();
        if (pending.crtc->cursorPlane()) {
            pending.crtc->cursorPlane()->rollbackPending();
        }
    }
}

void DrmPipeline::atomicCommitSuccessful(CommitMode mode)
{
    m_oldTestBuffer = nullptr;
    m_connector->commitPending();
    if (pending.crtc) {
        pending.crtc->commitPending();
        pending.crtc->primaryPlane()->commitPending();
        if (pending.crtc->cursorPlane()) {
            pending.crtc->cursorPlane()->commitPending();
        }
    }
    if (mode != CommitMode::Test) {
        if (activePending()) {
            m_pageflipPending = true;
        }
        m_connector->commit();
        if (pending.crtc) {
            pending.crtc->commit();
            pending.crtc->primaryPlane()->setNext(m_primaryBuffer);
            pending.crtc->primaryPlane()->commit();
            if (pending.crtc->cursorPlane()) {
                pending.crtc->cursorPlane()->setNext(pending.cursorBo);
                pending.crtc->cursorPlane()->commit();
            }
        }
        m_current = pending;
        if (mode == CommitMode::CommitModeset && activePending()) {
            pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
        }
    }
}

bool DrmPipeline::checkTestBuffer()
{
    if (!pending.crtc || (m_primaryBuffer && m_primaryBuffer->size() == bufferSize())) {
        return true;
    }
    auto backend = gpu()->eglBackend();
    QSharedPointer<DrmBuffer> buffer;
    // try to re-use buffers if possible.
    const auto &checkBuffer = [this, backend, &buffer](const QSharedPointer<DrmBuffer> &buf){
        const auto &mods = supportedModifiers(buf->format());
        if (backend && buf->format() == backend->drmFormat(m_output)
            && (mods.isEmpty() || mods.contains(buf->modifier()))
            && buf->size() == bufferSize()) {
            buffer = buf;
        }
    };
    if (pending.crtc->primaryPlane() && pending.crtc->primaryPlane()->next()) {
        checkBuffer(pending.crtc->primaryPlane()->next());
    } else if (pending.crtc->primaryPlane() && pending.crtc->primaryPlane()->current()) {
        checkBuffer(pending.crtc->primaryPlane()->current());
    } else if (pending.crtc->next()) {
        checkBuffer(pending.crtc->next());
    } else if (pending.crtc->current()) {
        checkBuffer(pending.crtc->current());
    }
    // if we don't have a fitting buffer already, get or create one
    if (!buffer) {
        if (backend && m_output) {
            buffer = backend->renderTestFrame(m_output);
        } else if (backend && gpu()->gbmDevice()) {
            gbm_bo *bo = gbm_bo_create(gpu()->gbmDevice(), bufferSize().width(), bufferSize().height(), DRM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
            if (!bo) {
                return false;
            }
            buffer = QSharedPointer<DrmGbmBuffer>::create(gpu(), bo, nullptr);
        } else {
            buffer = QSharedPointer<DrmDumbBuffer>::create(gpu(), bufferSize(), DRM_FORMAT_XRGB8888);
        }
    }
    if (buffer && buffer->bufferId()) {
        m_oldTestBuffer = m_primaryBuffer;
        m_primaryBuffer = buffer;
        return true;
    }
    return false;
}

bool DrmPipeline::setCursor(const QSharedPointer<DrmDumbBuffer> &buffer, const QPoint &hotspot)
{
    if (pending.cursorBo == buffer && pending.cursorHotspot == hotspot) {
        return true;
    }
    bool result;
    const bool visibleBefore = isCursorVisible();
    pending.cursorBo = buffer;
    pending.cursorHotspot = hotspot;
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (pending.crtc->cursorPlane()) {
        result = commitPipelines({this}, CommitMode::Test);
    } else {
        result = setCursorLegacy();
    }
    if (result) {
        m_next = pending;
        if (m_output && (visibleBefore || isCursorVisible())) {
            m_output->renderLoop()->scheduleRepaint();
        }
    } else {
        pending = m_next;
    }
    return result;
}

bool DrmPipeline::moveCursor(QPoint pos)
{
    if (pending.cursorPos == pos) {
        return true;
    }
    const bool visibleBefore = isCursorVisible();
    bool result;
    pending.cursorPos = pos;
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (pending.crtc->cursorPlane()) {
        result = commitPipelines({this}, CommitMode::Test);
    } else {
        result = moveCursorLegacy();
    }
    if (result) {
        m_next = pending;
        if (m_output && (visibleBefore || isCursorVisible())) {
            m_output->renderLoop()->scheduleRepaint();
        }
    } else {
        pending = m_next;
    }
    return result;
}

void DrmPipeline::applyPendingChanges()
{
    if (!pending.crtc) {
        pending.active = false;
    }
    m_next = pending;
}

QSize DrmPipeline::bufferSize() const
{
    const auto modeSize = m_connector->modes().at(pending.modeIndex)->size();
    if (pending.bufferTransformation & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) {
        return modeSize.transposed();
    }
    return modeSize;
}

QSize DrmPipeline::sourceSize() const
{
    const auto modeSize = m_connector->modes().at(pending.modeIndex)->size();
    if (pending.sourceTransformation & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) {
        return modeSize.transposed();
    }
    return modeSize;
}

bool DrmPipeline::isCursorVisible() const
{
    const QRect mode = QRect(QPoint(), m_connector->modes().at(pending.modeIndex)->size());
    return pending.cursorBo && QRect(pending.cursorPos, pending.cursorBo->size()).intersects(mode);
}

DrmConnector *DrmPipeline::connector() const
{
    return m_connector;
}

DrmGpu *DrmPipeline::gpu() const
{
    return m_connector->gpu();
}

void DrmPipeline::pageFlipped(std::chrono::nanoseconds timestamp)
{
    m_current.crtc->flipBuffer();
    if (m_current.crtc->primaryPlane()) {
        m_current.crtc->primaryPlane()->flipBuffer();
    }
    if (m_current.crtc->cursorPlane()) {
        m_current.crtc->cursorPlane()->flipBuffer();
    }
    m_pageflipPending = false;
    if (m_output) {
        m_output->pageFlipped(timestamp);
    }
}

void DrmPipeline::setOutput(DrmOutput *output)
{
    m_output = output;
}

DrmOutput *DrmPipeline::output() const
{
    return m_output;
}

static const QMap<uint32_t, QVector<uint64_t>> legacyFormats = {
    {DRM_FORMAT_XRGB8888, {}}
};

bool DrmPipeline::isFormatSupported(uint32_t drmFormat) const
{
    if (pending.crtc) {
        if (pending.crtc->primaryPlane()) {
            return pending.crtc->primaryPlane()->formats().contains(drmFormat);
        } else {
            return legacyFormats.contains(drmFormat);
        }
    } else {
        return false;
    }
}

QVector<uint64_t> DrmPipeline::supportedModifiers(uint32_t drmFormat) const
{
    if (pending.crtc && pending.crtc->primaryPlane()) {
        return pending.crtc->primaryPlane()->formats().value(drmFormat);
    } else {
        return {};
    }
}

QMap<uint32_t, QVector<uint64_t>> DrmPipeline::supportedFormats() const
{
    if (pending.crtc) {
        if (pending.crtc->primaryPlane()) {
            return pending.crtc->primaryPlane()->formats();
        } else {
            return legacyFormats;
        }
    } else {
        return {};
    }
}

bool DrmPipeline::needsModeset() const
{
    return pending.crtc != m_current.crtc
        || pending.active != m_current.active
        || pending.modeIndex != m_current.modeIndex
        || pending.rgbRange != m_current.rgbRange
        || pending.bufferTransformation != m_current.bufferTransformation
        || m_connector->linkStatus() == DrmConnector::LinkStatus::Bad
        || m_modesetPresentPending;
}

bool DrmPipeline::activePending() const
{
    return pending.crtc && pending.active;
}

void DrmPipeline::revertPendingChanges()
{
    pending = m_next;
}

bool DrmPipeline::pageflipPending() const
{
    return m_pageflipPending;
}

bool DrmPipeline::modesetPresentPending() const
{
    return m_modesetPresentPending;
}

void DrmPipeline::resetModesetPresentPending()
{
    m_modesetPresentPending = false;
}

DrmCrtc *DrmPipeline::currentCrtc() const
{
    return m_current.crtc;
}

DrmGammaRamp::DrmGammaRamp(DrmGpu *gpu, const GammaRamp &lut)
    : m_gpu(gpu)
    , m_lut(lut)
{
    if (gpu->atomicModeSetting()) {
        QVector<drm_color_lut> atomicLut(lut.size());
        for (uint32_t i = 0; i < lut.size(); i++) {
            atomicLut[i].red = lut.red()[i];
            atomicLut[i].green = lut.green()[i];
            atomicLut[i].blue = lut.blue()[i];
        }
        if (drmModeCreatePropertyBlob(gpu->fd(), atomicLut.data(), sizeof(drm_color_lut) * lut.size(), &m_blobId) != 0) {
            qCWarning(KWIN_DRM) << "Failed to create gamma blob!" << strerror(errno);
        }
    }
}

DrmGammaRamp::~DrmGammaRamp()
{
    if (m_blobId != 0) {
        drmModeDestroyPropertyBlob(m_gpu->fd(), m_blobId);
    }
}

uint32_t DrmGammaRamp::blobId() const
{
    return m_blobId;
}

uint32_t DrmGammaRamp::size() const
{
    return m_lut.size();
}

uint16_t *DrmGammaRamp::red() const
{
    return const_cast<uint16_t*>(m_lut.red());
}

uint16_t *DrmGammaRamp::green() const
{
    return const_cast<uint16_t*>(m_lut.green());
}

uint16_t *DrmGammaRamp::blue() const
{
    return const_cast<uint16_t*>(m_lut.blue());
}

void DrmPipeline::printFlags(uint32_t flags)
{
    if (flags == 0) {
        qCDebug(KWIN_DRM) << "Flags: none";
    } else {
        qCDebug(KWIN_DRM) << "Flags:";
        if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
            qCDebug(KWIN_DRM) << "\t DRM_MODE_PAGE_FLIP_EVENT";
        }
        if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET) {
            qCDebug(KWIN_DRM) << "\t DRM_MODE_ATOMIC_ALLOW_MODESET";
        }
        if (flags & DRM_MODE_PAGE_FLIP_ASYNC) {
            qCDebug(KWIN_DRM) << "\t DRM_MODE_PAGE_FLIP_ASYNC";
        }
    }
}

void DrmPipeline::printProps(DrmObject *object, PrintMode mode)
{
    auto list = object->properties();
    bool any = mode == PrintMode::All || std::any_of(list.constBegin(), list.constEnd(), [](const auto &prop){
        return prop && !prop->isImmutable() && prop->needsCommit();
    });
    if (!any) {
        return;
    }
    qCDebug(KWIN_DRM) << object->typeName() << object->id();
    for (const auto &prop : list) {
        if (prop) {
            uint64_t current = prop->name().startsWith("SRC_") ? prop->current() >> 16 : prop->current();
            if (prop->isImmutable() || !prop->needsCommit()) {
                if (mode == PrintMode::All) {
                    qCDebug(KWIN_DRM).nospace() << "\t" << prop->name() << ": " << current;
                }
            } else {
                uint64_t pending = prop->name().startsWith("SRC_") ? prop->pending() >> 16 : prop->pending();
                qCDebug(KWIN_DRM).nospace() << "\t" << prop->name() << ": " << current << "->" << pending;
            }
        }
    }
}

void DrmPipeline::printDebugInfo() const
{
    qCDebug(KWIN_DRM) << "Drm objects:";
    printProps(m_connector, PrintMode::All);
    if (pending.crtc) {
        printProps(pending.crtc, PrintMode::All);
        if (pending.crtc->primaryPlane()) {
            printProps(pending.crtc->primaryPlane(), PrintMode::All);
        }
        if (pending.crtc->cursorPlane()) {
            printProps(pending.crtc->cursorPlane(), PrintMode::All);
        }
    }
}

}
