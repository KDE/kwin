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
    if (!buffer) {
        if (m_output) {
            m_output->presentFailed();
        }
        return false;
    }
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
        drmModeAtomicReq *req = drmModeAtomicAlloc();
        if (!req) {
            qCDebug(KWIN_DRM) << "Failed to allocate drmModeAtomicReq!" << strerror(errno);
            return false;
        }
        uint32_t flags = 0;
        const auto &failed = [pipelines, req, mode, &flags, unusedObjects](){
            drmModeAtomicFree(req);
            printFlags(flags);
            for (const auto &pipeline : pipelines) {
                pipeline->printDebugInfo();
                if (pipeline->m_oldTestBuffer) {
                    pipeline->m_primaryBuffer = pipeline->m_oldTestBuffer;
                    pipeline->m_oldTestBuffer = nullptr;
                }
                pipeline->m_connector->rollbackPending();
                if (pipeline->pending.crtc) {
                    pipeline->pending.crtc->rollbackPending();
                    pipeline->pending.crtc->primaryPlane()->rollbackPending();
                }
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
            qCWarning(KWIN_DRM) << "Atomic test for" << mode << "failed!" << strerror(errno);
            return failed();
        }
        if (mode != CommitMode::Test && drmModeAtomicCommit(pipelines[0]->gpu()->fd(), req, flags, nullptr) != 0) {
            qCWarning(KWIN_DRM) << "Atomic commit failed! This should never happen!" << strerror(errno);
            return failed();
        }
        for (const auto &pipeline : pipelines) {
            pipeline->m_oldTestBuffer = nullptr;
            pipeline->m_connector->commitPending();
            if (pipeline->pending.crtc) {
                pipeline->pending.crtc->commitPending();
                pipeline->pending.crtc->primaryPlane()->commitPending();
                if (pipeline->pending.crtc->cursorPlane()) {
                    pipeline->pending.crtc->cursorPlane()->commitPending();
                }
            }
            if (mode != CommitMode::Test) {
                if (pipeline->activePending()) {
                    pipeline->m_pageflipPending = true;
                }
                pipeline->m_connector->commit();
                if (pipeline->pending.crtc) {
                    pipeline->pending.crtc->primaryPlane()->setNext(pipeline->m_primaryBuffer);
                    pipeline->pending.crtc->commit();
                    pipeline->pending.crtc->primaryPlane()->commit();
                    if (pipeline->pending.crtc->cursorPlane()) {
                        pipeline->pending.crtc->cursorPlane()->commit();
                    }
                }
                pipeline->m_current = pipeline->pending;
                if (modeset && pipeline->activePending()) {
                    pipeline->pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
                }
            }
        }
        for (const auto &obj : unusedObjects) {
            obj->commitPending();
            if (mode != CommitMode::Test) {
                obj->commit();
            }
        }
        drmModeAtomicFree(req);
        return true;
    } else {
        bool failure = false;
        for (const auto &pipeline : pipelines) {
            if (!pipeline->applyPendingChangesLegacy()) {
                failure = true;
                break;
            }
        }
        if (failure) {
            // at least try to revert the config
            for (const auto &pipeline : pipelines) {
                pipeline->revertPendingChanges();
                pipeline->applyPendingChangesLegacy();
            }
            return false;
        } else {
            for (const auto &pipeline : pipelines) {
                pipeline->applyPendingChanges();
                pipeline->m_current = pipeline->pending;
                if (mode == CommitMode::CommitModeset && mode != CommitMode::Test && pipeline->activePending()) {
                    pipeline->pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
                }
            }
            return true;
        }
    }
}

bool DrmPipeline::populateAtomicValues(drmModeAtomicReq *req, uint32_t &flags)
{
    if (needsModeset()) {
        prepareModeset();
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    }
    if (activePending()) {
        flags |= DRM_MODE_PAGE_FLIP_EVENT;
    }
    if (pending.crtc) {
        auto modeSize = m_connector->modes()[pending.modeIndex]->size();
        pending.crtc->primaryPlane()->set(QPoint(0, 0), m_primaryBuffer ? m_primaryBuffer->size() : modeSize, QPoint(0, 0), modeSize);
        pending.crtc->primaryPlane()->setBuffer(activePending() ? m_primaryBuffer.get() : nullptr);
        pending.crtc->setPending(DrmCrtc::PropertyIndex::VrrEnabled, pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive);
        pending.crtc->setPending(DrmCrtc::PropertyIndex::Gamma_LUT, pending.gamma ? pending.gamma->blobId() : 0);
    }
    return m_connector->atomicPopulate(req) && (!pending.crtc || (pending.crtc->atomicPopulate(req) && pending.crtc->primaryPlane()->atomicPopulate(req)));
}

bool DrmPipeline::presentLegacy()
{
    if ((!pending.crtc->current() || pending.crtc->current()->needsModeChange(m_primaryBuffer.get())) && !legacyModeset()) {
        return false;
    }
    QVector<DrmPipeline*> *userData = new QVector<DrmPipeline*>();
    *userData << this;
    if (drmModePageFlip(gpu()->fd(), pending.crtc->id(), m_primaryBuffer ? m_primaryBuffer->bufferId() : 0, DRM_MODE_PAGE_FLIP_EVENT, userData) != 0) {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno) << m_primaryBuffer;
        return false;
    }
    m_pageflipPending = true;
    pending.crtc->setNext(m_primaryBuffer);
    return true;
}

bool DrmPipeline::checkTestBuffer()
{
    if (!pending.crtc || (m_primaryBuffer && m_primaryBuffer->size() == sourceSize())) {
        return true;
    }
    auto backend = gpu()->eglBackend();
    QSharedPointer<DrmBuffer> buffer;
    // try to re-use buffers if possible.
    const auto &checkBuffer = [this, backend, &buffer](const QSharedPointer<DrmBuffer> &buf){
        const auto &mods = supportedModifiers(buf->format());
        if (backend && buf->format() == backend->drmFormat()
            && (mods.isEmpty() || mods.contains(buf->modifier()))
            && buf->size() == sourceSize()) {
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
            gbm_bo *bo = gbm_bo_create(gpu()->gbmDevice(), sourceSize().width(), sourceSize().height(), GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
            if (!bo) {
                return false;
            }
            buffer = QSharedPointer<DrmGbmBuffer>::create(gpu(), bo, nullptr);
        } else {
            buffer = QSharedPointer<DrmDumbBuffer>::create(gpu(), sourceSize());
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
    return pending.crtc->setLegacyCursor(buffer, hotspot);
}

bool DrmPipeline::moveCursor(QPoint pos)
{
    return pending.crtc->moveLegacyCursor(pos);
}

void DrmPipeline::prepareModeset()
{
    if (!pending.crtc) {
        m_connector->setPending(DrmConnector::PropertyIndex::CrtcId, 0);
        return;
    }
    auto mode = m_connector->modes()[pending.modeIndex];

    m_connector->setPending(DrmConnector::PropertyIndex::CrtcId, activePending() ? pending.crtc->id() : 0);
    if (const auto &prop = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
        prop->setEnum(pending.rgbRange);
    }

    pending.crtc->setPending(DrmCrtc::PropertyIndex::Active, activePending());
    pending.crtc->setPending(DrmCrtc::PropertyIndex::ModeId, activePending() ? mode->blobId() : 0);

    pending.crtc->primaryPlane()->setPending(DrmPlane::PropertyIndex::CrtcId, activePending() ? pending.crtc->id() : 0);
    pending.crtc->primaryPlane()->setTransformation(DrmPlane::Transformation::Rotate0);
    pending.crtc->primaryPlane()->set(QPoint(0, 0), sourceSize(), QPoint(0, 0), mode->size());
}

void DrmPipeline::applyPendingChanges()
{
    if (!pending.crtc) {
        pending.active = false;
    }
    m_next = pending;
}

bool DrmPipeline::applyPendingChangesLegacy()
{
    if (!pending.active && pending.crtc) {
        drmModeSetCursor(gpu()->fd(), pending.crtc->id(), 0, 0, 0);
    }
    if (pending.active) {
        Q_ASSERT(pending.crtc);
        if (auto vrr = pending.crtc->getProp(DrmCrtc::PropertyIndex::VrrEnabled); vrr && !vrr->setPropertyLegacy(pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive)) {
            qCWarning(KWIN_DRM) << "Setting vrr failed!" << strerror(errno);
            return false;
        }
        if (const auto &rgbRange = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
            rgbRange->setEnumLegacy(pending.rgbRange);
        }
        if (needsModeset() &&!legacyModeset()) {
            return false;
        }
        m_connector->getProp(DrmConnector::PropertyIndex::Dpms)->setCurrent(DRM_MODE_DPMS_ON);
        if (pending.gamma && drmModeCrtcSetGamma(gpu()->fd(), pending.crtc->id(), pending.gamma->size(),
                                                 pending.gamma->red(), pending.gamma->green(), pending.gamma->blue()) != 0) {
            qCWarning(KWIN_DRM) << "Setting gamma failed!" << strerror(errno);
            return false;
        }

        pending.crtc->setLegacyCursor();
    }
    if (!m_connector->getProp(DrmConnector::PropertyIndex::Dpms)->setPropertyLegacy(pending.active ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF)) {
        qCWarning(KWIN_DRM) << "Setting legacy dpms failed!" << strerror(errno);
        return false;
    }
    return true;
}

bool DrmPipeline::legacyModeset()
{
    auto mode = m_connector->modes()[pending.modeIndex];
    uint32_t connId = m_connector->id();
    if (!checkTestBuffer() || drmModeSetCrtc(gpu()->fd(), pending.crtc->id(), m_primaryBuffer->bufferId(), 0, 0, &connId, 1, mode->nativeMode()) != 0) {
        qCWarning(KWIN_DRM) << "Modeset failed!" << strerror(errno);
        pending = m_next;
        m_primaryBuffer = m_oldTestBuffer;
        return false;
    }
    m_oldTestBuffer = nullptr;
    // make sure the buffer gets kept alive, or the modeset gets reverted by the kernel
    if (pending.crtc->current()) {
        pending.crtc->setNext(m_primaryBuffer);
    } else {
        pending.crtc->setCurrent(m_primaryBuffer);
    }
    return true;
}

QSize DrmPipeline::sourceSize() const
{
    auto mode = m_connector->modes()[pending.modeIndex];
    if (pending.transformation & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) {
        return mode->size().transposed();
    }
    return mode->size();
}

bool DrmPipeline::isCursorVisible() const
{
    return pending.crtc && pending.crtc->isCursorVisible(QRect(QPoint(0, 0), m_connector->currentMode()->size()));
}

QPoint DrmPipeline::cursorPos() const
{
    return pending.crtc->cursorPos();
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

bool DrmPipeline::isFormatSupported(uint32_t drmFormat) const
{
    if (pending.crtc) {
        if (pending.crtc->primaryPlane()) {
            return pending.crtc->primaryPlane()->formats().contains(drmFormat);
        } else {
            return drmFormat == DRM_FORMAT_XRGB8888 || drmFormat == DRM_FORMAT_ARGB8888;
        }
    } else {
        return false;
    }
}

QVector<uint64_t> DrmPipeline::supportedModifiers(uint32_t drmFormat) const
{
    if (pending.crtc && pending.crtc->primaryPlane()) {
        return pending.crtc->primaryPlane()->formats()[drmFormat];
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
        || pending.transformation != m_current.transformation
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
        qCWarning(KWIN_DRM) << "Flags: none";
    } else {
        qCWarning(KWIN_DRM) << "Flags:";
        if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
            qCWarning(KWIN_DRM) << "\t DRM_MODE_PAGE_FLIP_EVENT";
        }
        if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET) {
            qCWarning(KWIN_DRM) << "\t DRM_MODE_ATOMIC_ALLOW_MODESET";
        }
        if (flags & DRM_MODE_PAGE_FLIP_ASYNC) {
            qCWarning(KWIN_DRM) << "\t DRM_MODE_PAGE_FLIP_ASYNC";
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
    qCWarning(KWIN_DRM) << object->typeName() << object->id();
    for (const auto &prop : list) {
        if (prop) {
            uint64_t current = prop->name().startsWith("SRC_") ? prop->current() >> 16 : prop->current();
            if (prop->isImmutable() || !prop->needsCommit()) {
                if (mode == PrintMode::All) {
                    qCWarning(KWIN_DRM).nospace() << "\t" << prop->name() << ": " << current;
                }
            } else {
                uint64_t pending = prop->name().startsWith("SRC_") ? prop->pending() >> 16 : prop->pending();
                qCWarning(KWIN_DRM).nospace() << "\t" << prop->name() << ": " << current << "->" << pending;
            }
        }
    }
}

void DrmPipeline::printDebugInfo() const
{
    qCWarning(KWIN_DRM) << "Drm objects:";
    printProps(m_connector, PrintMode::All);
    if (pending.crtc) {
        printProps(pending.crtc, PrintMode::All);
        if (pending.crtc->primaryPlane()) {
            printProps(pending.crtc->primaryPlane(), PrintMode::All);
        }
    }
}

}
