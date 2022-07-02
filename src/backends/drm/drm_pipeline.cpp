/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_pipeline.h"

#include <errno.h>

#include "cursor.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_buffer_gbm.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "drm_output.h"
#include "egl_gbm_backend.h"
#include "logging.h"
#include "session.h"

#include <drm_fourcc.h>
#include <gbm.h>

namespace KWin
{

static const QMap<uint32_t, QVector<uint64_t>> legacyFormats = {{DRM_FORMAT_XRGB8888, {}}};
static const QMap<uint32_t, QVector<uint64_t>> legacyCursorFormats = {{DRM_FORMAT_ARGB8888, {}}};

DrmPipeline::DrmPipeline(DrmConnector *conn)
    : m_connector(conn)
{
}

DrmPipeline::~DrmPipeline()
{
    if (m_pageflipPending && m_current.crtc) {
        pageFlipped({});
    }
}

bool DrmPipeline::testScanout()
{
    // TODO make the modeset check only be tested at most once per scanout cycle
    if (gpu()->needsModeset()) {
        return false;
    }
    if (gpu()->atomicModeSetting()) {
        return commitPipelines({this}, CommitMode::Test);
    } else {
        // no other way to test than to do it.
        // As we only have a maximum of one test per scanout cycle, this is fine
        return presentLegacy();
    }
}

bool DrmPipeline::present()
{
    Q_ASSERT(m_pending.crtc);
    if (gpu()->atomicModeSetting()) {
        return commitPipelines({this}, CommitMode::Commit);
    } else {
        if (m_pending.layer->hasDirectScanoutBuffer()) {
            // already presented
            return true;
        }
        if (!presentLegacy()) {
            return false;
        }
    }
    return true;
}

bool DrmPipeline::maybeModeset()
{
    m_modesetPresentPending = true;
    return gpu()->maybeModeset();
}

bool DrmPipeline::commitPipelines(const QVector<DrmPipeline *> &pipelines, CommitMode mode, const QVector<DrmObject *> &unusedObjects)
{
    Q_ASSERT(!pipelines.isEmpty());
    if (pipelines[0]->gpu()->atomicModeSetting()) {
        return commitPipelinesAtomic(pipelines, mode, unusedObjects);
    } else {
        return commitPipelinesLegacy(pipelines, mode);
    }
}

bool DrmPipeline::commitPipelinesAtomic(const QVector<DrmPipeline *> &pipelines, CommitMode mode, const QVector<DrmObject *> &unusedObjects)
{
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    if (!req) {
        qCCritical(KWIN_DRM) << "Failed to allocate drmModeAtomicReq!" << strerror(errno);
        return false;
    }
    uint32_t flags = 0;
    const auto &failed = [pipelines, req, &flags, unusedObjects]() {
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
        if (pipeline->activePending() && !pipeline->m_pending.layer->checkTestBuffer()) {
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
    bool modeset = needsModeset();
    if (modeset) {
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
        m_pending.needsModeset = true;
    }
    if (activePending()) {
        flags |= DRM_MODE_PAGE_FLIP_EVENT;
    }
    if (m_pending.needsModeset) {
        prepareAtomicModeset();
    }
    if (m_pending.crtc) {
        m_pending.crtc->setPending(DrmCrtc::PropertyIndex::VrrEnabled, m_pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive);
        m_pending.crtc->setPending(DrmCrtc::PropertyIndex::Gamma_LUT, m_pending.gamma ? m_pending.gamma->blobId() : 0);
        const auto modeSize = m_pending.mode->size();
        const auto fb = m_pending.layer->currentBuffer().get();
        m_pending.crtc->primaryPlane()->set(QPoint(0, 0), fb ? fb->buffer()->size() : bufferSize(), QPoint(0, 0), modeSize);
        m_pending.crtc->primaryPlane()->setBuffer(activePending() ? fb : nullptr);

        if (m_pending.crtc->cursorPlane()) {
            const auto layer = cursorLayer();
            bool active = activePending() && layer->isVisible();
            m_pending.crtc->cursorPlane()->set(QPoint(0, 0), gpu()->cursorSize(), layer->position(), gpu()->cursorSize());
            m_pending.crtc->cursorPlane()->setBuffer(active ? layer->currentBuffer().get() : nullptr);
            m_pending.crtc->cursorPlane()->setPending(DrmPlane::PropertyIndex::CrtcId, active ? m_pending.crtc->id() : 0);
        }
    }
    if (!m_connector->atomicPopulate(req)) {
        return false;
    }
    if (m_pending.crtc) {
        if (!m_pending.crtc->atomicPopulate(req)) {
            return false;
        }
        if (!m_pending.crtc->primaryPlane()->atomicPopulate(req)) {
            return false;
        }
        if (m_pending.crtc->cursorPlane() && !m_pending.crtc->cursorPlane()->atomicPopulate(req)) {
            return false;
        }
    }
    return true;
}

void DrmPipeline::prepareAtomicModeset()
{
    if (!m_pending.crtc) {
        m_connector->setPending(DrmConnector::PropertyIndex::CrtcId, 0);
        return;
    }

    m_connector->setPending(DrmConnector::PropertyIndex::CrtcId, activePending() ? m_pending.crtc->id() : 0);
    if (const auto &prop = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
        prop->setEnum(m_pending.rgbRange);
    }
    if (const auto &prop = m_connector->getProp(DrmConnector::PropertyIndex::LinkStatus)) {
        prop->setEnum(DrmConnector::LinkStatus::Good);
    }
    if (const auto overscan = m_connector->getProp(DrmConnector::PropertyIndex::Overscan)) {
        overscan->setPending(m_pending.overscan);
    } else if (const auto underscan = m_connector->getProp(DrmConnector::PropertyIndex::Underscan)) {
        const uint32_t hborder = calculateUnderscan();
        underscan->setEnum(m_pending.overscan != 0 ? DrmConnector::UnderscanOptions::On : DrmConnector::UnderscanOptions::Off);
        m_connector->getProp(DrmConnector::PropertyIndex::Underscan_vborder)->setPending(m_pending.overscan);
        m_connector->getProp(DrmConnector::PropertyIndex::Underscan_hborder)->setPending(hborder);
    }
    if (const auto bpc = m_connector->getProp(DrmConnector::PropertyIndex::MaxBpc)) {
        uint64_t preferred = 8;
        if (auto backend = dynamic_cast<EglGbmBackend *>(gpu()->platform()->renderBackend()); backend && backend->prefer10bpc()) {
            preferred = 10;
        }
        bpc->setPending(std::min(bpc->maxValue(), preferred));
    }

    m_pending.crtc->setPending(DrmCrtc::PropertyIndex::Active, activePending());
    m_pending.crtc->setPending(DrmCrtc::PropertyIndex::ModeId, activePending() ? m_pending.mode->blobId() : 0);

    m_pending.crtc->primaryPlane()->setPending(DrmPlane::PropertyIndex::CrtcId, activePending() ? m_pending.crtc->id() : 0);
    m_pending.crtc->primaryPlane()->setTransformation(m_pending.bufferOrientation);
    if (m_pending.crtc->cursorPlane()) {
        m_pending.crtc->cursorPlane()->setTransformation(DrmPlane::Transformation::Rotate0);
    }
}

uint32_t DrmPipeline::calculateUnderscan()
{
    const auto size = m_pending.mode->size();
    const float aspectRatio = size.width() / static_cast<float>(size.height());
    uint32_t hborder = m_pending.overscan * aspectRatio;
    if (hborder > 128) {
        // overscan only goes from 0-100 so we cut off the 101-128 value range of underscan_vborder
        hborder = 128;
        m_pending.overscan = 128 / aspectRatio;
    }
    return hborder;
}

void DrmPipeline::atomicCommitFailed()
{
    m_connector->rollbackPending();
    if (m_pending.crtc) {
        m_pending.crtc->rollbackPending();
        m_pending.crtc->primaryPlane()->rollbackPending();
        if (m_pending.crtc->cursorPlane()) {
            m_pending.crtc->cursorPlane()->rollbackPending();
        }
    }
}

void DrmPipeline::atomicCommitSuccessful(CommitMode mode)
{
    m_connector->commitPending();
    if (m_pending.crtc) {
        m_pending.crtc->commitPending();
        m_pending.crtc->primaryPlane()->commitPending();
        if (m_pending.crtc->cursorPlane()) {
            m_pending.crtc->cursorPlane()->commitPending();
        }
    }
    if (mode != CommitMode::Test) {
        m_pending.needsModeset = false;
        if (activePending()) {
            m_pageflipPending = true;
        }
        m_connector->commit();
        if (m_pending.crtc) {
            m_pending.crtc->commit();
            m_pending.crtc->primaryPlane()->setNext(m_pending.layer->currentBuffer());
            m_pending.crtc->primaryPlane()->commit();
            if (m_pending.crtc->cursorPlane()) {
                m_pending.crtc->cursorPlane()->setNext(cursorLayer()->currentBuffer());
                m_pending.crtc->cursorPlane()->commit();
            }
        }
        m_current = m_pending;
        if (mode == CommitMode::CommitModeset && activePending()) {
            pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
        }
    }
}

bool DrmPipeline::setCursor(const QPoint &hotspot)
{
    bool result;
    m_pending.cursorHotspot = hotspot;
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (m_pending.crtc->cursorPlane()) {
        result = commitPipelines({this}, CommitMode::Test);
        if (result && m_output) {
            m_output->renderLoop()->scheduleRepaint();
        }
    } else {
        result = setCursorLegacy();
    }
    if (result) {
        m_next = m_pending;
    } else {
        m_pending = m_next;
    }
    return result;
}

bool DrmPipeline::moveCursor()
{
    bool result;
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (m_pending.crtc->cursorPlane()) {
        result = commitPipelines({this}, CommitMode::Test);
    } else {
        result = moveCursorLegacy();
    }
    if (result) {
        m_next = m_pending;
        if (m_output) {
            m_output->renderLoop()->scheduleRepaint();
        }
    } else {
        m_pending = m_next;
    }
    return result;
}

void DrmPipeline::applyPendingChanges()
{
    if (!m_pending.crtc) {
        m_pending.active = false;
    }
    m_next = m_pending;
}

QSize DrmPipeline::bufferSize() const
{
    const auto modeSize = m_pending.mode->size();
    if (m_pending.bufferOrientation & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) {
        return modeSize.transposed();
    }
    return modeSize;
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

QMap<uint32_t, QVector<uint64_t>> DrmPipeline::formats() const
{
    return m_pending.formats;
}

QMap<uint32_t, QVector<uint64_t>> DrmPipeline::cursorFormats() const
{
    if (m_pending.crtc && m_pending.crtc->cursorPlane()) {
        return m_pending.crtc->cursorPlane()->formats();
    } else {
        return legacyCursorFormats;
    }
}

bool DrmPipeline::pruneModifier()
{
    if (!m_pending.layer->currentBuffer()
        || m_pending.layer->currentBuffer()->buffer()->modifier() == DRM_FORMAT_MOD_NONE
        || m_pending.layer->currentBuffer()->buffer()->modifier() == DRM_FORMAT_MOD_INVALID) {
        return false;
    }
    auto &modifiers = m_pending.formats[m_pending.layer->currentBuffer()->buffer()->format()];
    if (modifiers.count() <= 1) {
        return false;
    }
    modifiers.removeOne(m_pending.layer->currentBuffer()->buffer()->modifier());
    return true;
}

bool DrmPipeline::needsModeset() const
{
    if (m_connector->needsModeset()) {
        return true;
    }
    if (m_pending.crtc) {
        if (m_pending.crtc->needsModeset()) {
            return true;
        }
        if (auto primary = m_pending.crtc->primaryPlane(); primary && primary->needsModeset()) {
            return true;
        }
        if (auto cursor = m_pending.crtc->cursorPlane(); cursor && cursor->needsModeset()) {
            return true;
        }
    }
    return m_pending.crtc != m_current.crtc
        || m_pending.active != m_current.active
        || m_pending.mode != m_current.mode
        || m_pending.rgbRange != m_current.rgbRange
        || m_pending.bufferOrientation != m_current.bufferOrientation
        || m_connector->linkStatus() == DrmConnector::LinkStatus::Bad
        || m_modesetPresentPending;
}

bool DrmPipeline::activePending() const
{
    return m_pending.crtc && m_pending.mode && m_pending.active;
}

void DrmPipeline::revertPendingChanges()
{
    m_pending = m_next;
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

DrmGammaRamp::DrmGammaRamp(DrmCrtc *crtc, const QSharedPointer<ColorTransformation> &transformation)
    : m_gpu(crtc->gpu())
    , m_lut(transformation, crtc->gammaRampSize())
{
    if (crtc->gpu()->atomicModeSetting()) {
        QVector<drm_color_lut> atomicLut(m_lut.size());
        for (uint32_t i = 0; i < m_lut.size(); i++) {
            atomicLut[i].red = m_lut.red()[i];
            atomicLut[i].green = m_lut.green()[i];
            atomicLut[i].blue = m_lut.blue()[i];
        }
        if (drmModeCreatePropertyBlob(crtc->gpu()->fd(), atomicLut.data(), sizeof(drm_color_lut) * m_lut.size(), &m_blobId) != 0) {
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

const ColorLUT &DrmGammaRamp::lut() const
{
    return m_lut;
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
    bool any = mode == PrintMode::All || std::any_of(list.constBegin(), list.constEnd(), [](const auto &prop) {
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
    if (m_pending.crtc) {
        printProps(m_pending.crtc, PrintMode::All);
        if (m_pending.crtc->primaryPlane()) {
            printProps(m_pending.crtc->primaryPlane(), PrintMode::All);
        }
        if (m_pending.crtc->cursorPlane()) {
            printProps(m_pending.crtc->cursorPlane(), PrintMode::All);
        }
    }
}

DrmCrtc *DrmPipeline::crtc() const
{
    return m_pending.crtc;
}

QSharedPointer<DrmConnectorMode> DrmPipeline::mode() const
{
    return m_pending.mode;
}

bool DrmPipeline::active() const
{
    return m_pending.active;
}

bool DrmPipeline::enabled() const
{
    return m_pending.enabled;
}

DrmPipelineLayer *DrmPipeline::primaryLayer() const
{
    return m_pending.layer.get();
}

DrmOverlayLayer *DrmPipeline::cursorLayer() const
{
    return m_pending.cursorLayer.get();
}

DrmPlane::Transformations DrmPipeline::renderOrientation() const
{
    return m_pending.renderOrientation;
}

DrmPlane::Transformations DrmPipeline::bufferOrientation() const
{
    return m_pending.bufferOrientation;
}

RenderLoopPrivate::SyncMode DrmPipeline::syncMode() const
{
    return m_pending.syncMode;
}

uint32_t DrmPipeline::overscan() const
{
    return m_pending.overscan;
}

Output::RgbRange DrmPipeline::rgbRange() const
{
    return m_pending.rgbRange;
}

void DrmPipeline::setCrtc(DrmCrtc *crtc)
{
    if (crtc && m_pending.crtc && crtc->gammaRampSize() != m_pending.crtc->gammaRampSize() && m_pending.colorTransformation) {
        m_pending.gamma = QSharedPointer<DrmGammaRamp>::create(crtc, m_pending.colorTransformation);
    }
    m_pending.crtc = crtc;
    if (crtc) {
        m_pending.formats = crtc->primaryPlane() ? crtc->primaryPlane()->formats() : legacyFormats;
    } else {
        m_pending.formats = {};
    }
}

void DrmPipeline::setMode(const QSharedPointer<DrmConnectorMode> &mode)
{
    m_pending.mode = mode;
}

void DrmPipeline::setActive(bool active)
{
    m_pending.active = active;
}

void DrmPipeline::setEnable(bool enable)
{
    m_pending.enabled = enable;
}

void DrmPipeline::setLayers(const QSharedPointer<DrmPipelineLayer> &primaryLayer, const QSharedPointer<DrmOverlayLayer> &cursorLayer)
{
    m_pending.layer = primaryLayer;
    m_pending.cursorLayer = cursorLayer;
}

void DrmPipeline::setRenderOrientation(DrmPlane::Transformations orientation)
{
    m_pending.renderOrientation = orientation;
}

void DrmPipeline::setBufferOrientation(DrmPlane::Transformations orientation)
{
    m_pending.bufferOrientation = orientation;
}

void DrmPipeline::setSyncMode(RenderLoopPrivate::SyncMode mode)
{
    m_pending.syncMode = mode;
}

void DrmPipeline::setOverscan(uint32_t overscan)
{
    m_pending.overscan = overscan;
}

void DrmPipeline::setRgbRange(Output::RgbRange range)
{
    m_pending.rgbRange = range;
}

void DrmPipeline::setColorTransformation(const QSharedPointer<ColorTransformation> &transformation)
{
    m_pending.colorTransformation = transformation;
    m_pending.gamma = QSharedPointer<DrmGammaRamp>::create(m_pending.crtc, transformation);
}
}
