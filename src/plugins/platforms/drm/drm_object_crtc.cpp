/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object_crtc.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "drm_buffer.h"
#include "drm_pointer.h"
#include "logging.h"
#include "drm_gpu.h"
#include <cerrno>

namespace KWin
{

DrmCrtc::DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex, DrmPlane *primaryPlane)
    : DrmObject(gpu, crtcId, {
        PropertyDefinition(QByteArrayLiteral("MODE_ID"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("ACTIVE"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("VRR_ENABLED"), Requirement::Optional),
        PropertyDefinition(QByteArrayLiteral("GAMMA_LUT"), Requirement::Optional),
    }, DRM_MODE_OBJECT_CRTC)
    , m_crtc(drmModeGetCrtc(gpu->fd(), crtcId))
    , m_pipeIndex(pipeIndex)
    , m_primaryPlane(primaryPlane)
{
}

bool DrmCrtc::init()
{
    if (!m_crtc) {
        return false;
    }
    return initProps();
}

void DrmCrtc::flipBuffer()
{
    m_currentBuffer = m_nextBuffer;
    m_nextBuffer = nullptr;
}

drmModeModeInfo DrmCrtc::queryCurrentMode()
{
    m_crtc.reset(drmModeGetCrtc(gpu()->fd(), id()));
    return m_crtc->mode;
}

bool DrmCrtc::needsModeset() const
{
    return getProp(PropertyIndex::Active)->needsCommit()
        || getProp(PropertyIndex::ModeId)->needsCommit();
}

int DrmCrtc::pipeIndex() const
{
    return m_pipeIndex;
}

QSharedPointer<DrmBuffer> DrmCrtc::current() const
{
    return m_currentBuffer;
}
QSharedPointer<DrmBuffer> DrmCrtc::next() const
{
    return m_nextBuffer;
}
void DrmCrtc::setCurrent(const QSharedPointer<DrmBuffer> &buffer)
{
    m_currentBuffer = buffer;
}
void DrmCrtc::setNext(const QSharedPointer<DrmBuffer> &buffer)
{
    m_nextBuffer = buffer;
}

int DrmCrtc::gammaRampSize() const
{
    return m_crtc->gamma_size;
}

bool DrmCrtc::setLegacyCursor(const QSharedPointer<DrmDumbBuffer> buffer, const QPoint &hotspot)
{
    if (m_cursor.bufferDirty || m_cursor.buffer != buffer || m_cursor.hotspot != hotspot) {
        const QSize &s = buffer ? buffer->size() : QSize(64, 64);
        int ret = drmModeSetCursor2(gpu()->fd(), id(), buffer ? buffer->handle() : 0, s.width(), s.height(), hotspot.x(), hotspot.y());
        if (ret == -ENOTSUP) {
            // for NVIDIA case that does not support drmModeSetCursor2
            ret = drmModeSetCursor(gpu()->fd(), id(), buffer ? buffer->handle() : 0, s.width(), s.height());
        }
        if (ret != 0) {
            qCWarning(KWIN_DRM) << "Could not set cursor:" << strerror(errno);
            return false;
        }
        m_cursor.buffer = buffer;
        m_cursor.bufferDirty = false;
        m_cursor.hotspot = hotspot;
    }
    return true;
}

bool DrmCrtc::moveLegacyCursor(const QPoint &pos)
{
    if (m_cursor.posDirty || m_cursor.pos != pos) {
        if (drmModeMoveCursor(gpu()->fd(), id(), pos.x(), pos.y()) != 0) {
            return false;
        }
        m_cursor.pos = pos;
        m_cursor.posDirty = false;
    }
    return true;
}

void DrmCrtc::setLegacyCursor()
{
    m_cursor.bufferDirty = true;
    m_cursor.posDirty = true;
    setLegacyCursor(m_cursor.buffer, m_cursor.hotspot);
    moveLegacyCursor(m_cursor.pos);
}

bool DrmCrtc::isCursorVisible(const QRect &output) const
{
    return m_cursor.buffer && QRect(m_cursor.pos, m_cursor.buffer->size()).intersects(output);
}

QPoint DrmCrtc::cursorPos() const
{
    return m_cursor.pos;
}

DrmPlane *DrmCrtc::primaryPlane() const
{
    return m_primaryPlane;
}

}
