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
#include "drm_object_plane.h"

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSize>
#include <QVector>
#include <xf86drmMode.h>

#include <KWayland/Server/outputdevice_interface.h>

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
class DrmDumbBuffer;
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
    void releaseGbm();
    void showCursor(DrmDumbBuffer *buffer);
    void showCursor();
    void hideCursor();
    void updateCursor();
    void moveCursor(const QPoint &globalPos);
    bool init(drmModeConnector *connector);
    bool present(DrmBuffer *buffer);
    void pageFlipped();

    /**
     * Enable or disable the output.
     * This differs from setDpms as it also
     * removes the wl_output
     * The default is on
     */
    void setEnabled(bool enabled);
    bool isEnabled() const;

    /**
     * This sets the changes and tests them against the DRM output
     */
    void setChanges(KWayland::Server::OutputChangeSet *changeset);
    bool commitChanges();

    QSize pixelSize() const;
    qreal scale() const;

    /*
     * The geometry of this output in global compositor co-ordinates (i.e scaled)
     */
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
        // We care for current as well as pending mode in order to allow first present in AMS.
        return m_dpmsModePending == DpmsMode::On;
    }

    QByteArray uuid() const {
        return m_uuid;
    }

    QSize physicalSize() const;

    bool initCursor(const QSize &cursorSize);

    bool supportsTransformations() const;

    bool isInternal() const {
        return m_internal;
    }

Q_SIGNALS:
    void dpmsChanged();
    void modeChanged();

private:
    friend class DrmBackend;
    friend class DrmCrtc;   // TODO: For use of setModeLegacy. Remove later when we allow multiple connectors per crtc
                            //       and save the connector ids in the DrmCrtc instance.
    DrmOutput(DrmBackend *backend);
    bool presentAtomically(DrmBuffer *buffer);

    enum class AtomicCommitMode {
        Test,
        Real
    };
    bool doAtomicCommit(AtomicCommitMode mode);

    bool presentLegacy(DrmBuffer *buffer);
    bool setModeLegacy(DrmBuffer *buffer);
    void initEdid(drmModeConnector *connector);
    void initDpms(drmModeConnector *connector);
    void initOutputDevice(drmModeConnector *connector);

    bool isCurrentMode(const drmModeModeInfo *mode) const;
    void initUuid();
    void setGlobalPos(const QPoint &pos);
    void setScale(qreal scale);
    void initOutput();
    bool initPrimaryPlane();
    bool initCursorPlane();

    void dpmsOnHandler();
    void dpmsOffHandler();
    bool dpmsAtomicOff();
    bool atomicReqModesetPopulate(drmModeAtomicReq *req, bool enable);
    void updateMode(int modeIndex);

    void transform(KWayland::Server::OutputDeviceInterface::Transform transform);
    void automaticRotation();

    DrmBackend *m_backend;
    DrmConnector *m_conn = nullptr;
    DrmCrtc *m_crtc = nullptr;
    QPoint m_globalPos;
    qreal m_scale = 1;
    bool m_lastGbm = false;
    drmModeModeInfo m_mode;
    Edid m_edid;
    QPointer<KWayland::Server::OutputInterface> m_waylandOutput;
    QPointer<KWayland::Server::OutputDeviceInterface> m_waylandOutputDevice;
    QPointer<KWayland::Server::OutputChangeSet> m_changeset;
    KWin::ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> m_dpms;
    DpmsMode m_dpmsMode = DpmsMode::On;
    DpmsMode m_dpmsModePending = DpmsMode::On;
    QByteArray m_uuid;

    uint32_t m_blobId = 0;
    DrmPlane* m_primaryPlane = nullptr;
    DrmPlane* m_cursorPlane = nullptr;
    QVector<DrmPlane*> m_nextPlanesFlipList;
    bool m_pageFlipPending = false;
    bool m_dpmsAtomicOffPending = false;
    bool m_modesetRequested = true;
    QSize m_physicalSize;
    Qt::ScreenOrientation m_orientation = Qt::PrimaryOrientation;

    struct {
        Qt::ScreenOrientation orientation;
        drmModeModeInfo mode;
        DrmPlane::Transformations planeTransformations;
        QPoint globalPos;
        bool valid = false;
    } m_lastWorkingState;
    DrmDumbBuffer *m_cursor[2] = {nullptr, nullptr};
    int m_cursorIndex = 0;
    bool m_hasNewCursor = false;
    bool m_internal = false;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput*)

#endif

