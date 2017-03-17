/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_DRM_OUTPUT_H
#define KWIN_DRM_OUTPUT_H

#include "drm_pointer.h"
#include "drm_object.h"

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSize>
#include <QVector>
#include <xf86drmMode.h>

namespace KWayland
{
namespace Server
{
class OutputInterface;
class OutputDeviceInterface;
class OutputChangeSet;
class OutputManagementInterface;
}
}

namespace KWin
{

class DrmBackend;
class DrmBuffer;
class DrmPlane;
class DrmConnector;
class DrmCrtc;

class DrmOutput : public QObject
{
    Q_OBJECT
public:
    struct Edid {
        QByteArray eisaId;
        QByteArray monitorName;
        QByteArray serialNumber;
        QSize physicalSize;
    };
    virtual ~DrmOutput();
    void cleanup();
    void showCursor(DrmBuffer *buffer);
    void hideCursor();
    void moveCursor(const QPoint &globalPos);
    bool init(drmModeConnector *connector);
    bool present(DrmBuffer *buffer);
    void pageFlipped();
    void restoreSaved();
    bool blank();

    /**
     * This sets the changes and tests them against the DRM output
     */
    void setChanges(KWayland::Server::OutputChangeSet *changeset);
    bool commitChanges();

    QSize size() const;
    QRect geometry() const;
    QString name() const;
    int currentRefreshRate() const;
    // These values are defined by the kernel
    enum class DpmsMode {
        On = DRM_MODE_DPMS_ON,
        Standby = DRM_MODE_DPMS_STANDBY,
        Suspend = DRM_MODE_DPMS_SUSPEND,
        Off = DRM_MODE_DPMS_OFF
    };
    void setDpms(DpmsMode mode);
    bool isDpmsEnabled() const {
        return m_dpmsMode == DpmsMode::On;
    }

    QByteArray uuid() const {
        return m_uuid;
    }

Q_SIGNALS:
    void dpmsChanged();

private:
    friend class DrmBackend;
    DrmOutput(DrmBackend *backend);
    void cleanupBlackBuffer();
    bool presentAtomically(DrmBuffer *buffer);
    bool presentLegacy(DrmBuffer *buffer);
    bool setModeLegacy(DrmBuffer *buffer);
    void initEdid(drmModeConnector *connector);
    void initDpms(drmModeConnector *connector);
    bool isCurrentMode(const drmModeModeInfo *mode) const;
    void initUuid();
    void setGlobalPos(const QPoint &pos);

    void pageFlippedBufferRemover(DrmBuffer *oldbuffer, DrmBuffer *newbuffer);
    bool initPrimaryPlane();
    bool initCursorPlane();
    DrmObject::AtomicReturn atomicReqModesetPopulate(drmModeAtomicReq *req, bool enable);

    DrmBackend *m_backend;
    QPoint m_globalPos;
    quint32 m_crtcId = 0;
    quint32 m_connector = 0;
    quint32 m_lastStride = 0;
    bool m_lastGbm = false;
    drmModeModeInfo m_mode;
    DrmBuffer *m_currentBuffer = nullptr;
    DrmBuffer *m_nextBuffer = nullptr;
    DrmBuffer *m_blackBuffer = nullptr;
    struct CrtcCleanup {
        static void inline cleanup(_drmModeCrtc *ptr) {
            drmModeFreeCrtc(ptr);       // TODO: Atomically? See compositor-drm.c l.3670
        }
    };
    Edid m_edid;
    QScopedPointer<_drmModeCrtc, CrtcCleanup> m_savedCrtc;
    QPointer<KWayland::Server::OutputInterface> m_waylandOutput;
    QPointer<KWayland::Server::OutputDeviceInterface> m_waylandOutputDevice;
    QPointer<KWayland::Server::OutputChangeSet> m_changeset;
    KWin::ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> m_dpms;
    DpmsMode m_dpmsMode = DpmsMode::On;
    QByteArray m_uuid;

    DrmConnector *m_conn = nullptr;
    DrmCrtc *m_crtc = nullptr;
    uint32_t m_blobId = 0;
    DrmPlane* m_primaryPlane = nullptr;
    DrmPlane* m_cursorPlane = nullptr;
    QVector<DrmPlane*> m_planesFlipList;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput*)

#endif

