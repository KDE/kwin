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
#include "drm_output.h"
#include "drm_backend.h"
#include "drm_object_plane.h"
#include "drm_object_crtc.h"
#include "drm_object_connector.h"

#include <errno.h>

#include "composite.h"
#include "logind.h"
#include "logging.h"
#include "main.h"
#include "screens_drm.h"
#include "wayland_server.h"
// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/outputchangeset.h>
#include <KWayland/Server/outputdevice_interface.h>
#include <KWayland/Server/outputmanagement_interface.h>
#include <KWayland/Server/outputconfiguration_interface.h>
// KF5
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
// Qt
#include <QCryptographicHash>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>


namespace KWin
{

DrmOutput::DrmOutput(DrmBackend *backend)
    : QObject()
    , m_backend(backend)
{
}

DrmOutput::~DrmOutput()
{
    hideCursor();
    cleanupBlackBuffer();
    delete m_crtc;
    delete m_conn;
    delete m_waylandOutput.data();
    delete m_waylandOutputDevice.data();
}

void DrmOutput::hideCursor()
{
    drmModeSetCursor(m_backend->fd(), m_crtcId, 0, 0, 0);
}

void DrmOutput::restoreSaved()
{
    if (!m_savedCrtc.isNull()) {
        drmModeSetCrtc(m_backend->fd(), m_savedCrtc->crtc_id, m_savedCrtc->buffer_id,
                       m_savedCrtc->x, m_savedCrtc->y, &m_connector, 1, &m_savedCrtc->mode);
    }
}

void DrmOutput::showCursor(DrmBuffer *c)
{
    const QSize &s = c->size();
    drmModeSetCursor(m_backend->fd(), m_crtcId, c->handle(), s.width(), s.height());
}

void DrmOutput::moveCursor(const QPoint &globalPos)
{
    const QPoint p = globalPos - m_globalPos;
    drmModeMoveCursor(m_backend->fd(), m_crtcId, p.x(), p.y());
}

QSize DrmOutput::size() const
{
    return QSize(m_mode.hdisplay, m_mode.vdisplay);
}

QRect DrmOutput::geometry() const
{
    return QRect(m_globalPos, size());
}

static KWayland::Server::OutputInterface::DpmsMode toWaylandDpmsMode(DrmOutput::DpmsMode mode)
{
    using namespace KWayland::Server;
    switch (mode) {
    case DrmOutput::DpmsMode::On:
        return OutputInterface::DpmsMode::On;
    case DrmOutput::DpmsMode::Standby:
        return OutputInterface::DpmsMode::Standby;
    case DrmOutput::DpmsMode::Suspend:
        return OutputInterface::DpmsMode::Suspend;
    case DrmOutput::DpmsMode::Off:
        return OutputInterface::DpmsMode::Off;
    default:
        Q_UNREACHABLE();
    }
}

static DrmOutput::DpmsMode fromWaylandDpmsMode(KWayland::Server::OutputInterface::DpmsMode wlMode)
{
    using namespace KWayland::Server;
    switch (wlMode) {
    case OutputInterface::DpmsMode::On:
        return DrmOutput::DpmsMode::On;
    case OutputInterface::DpmsMode::Standby:
        return DrmOutput::DpmsMode::Standby;
    case OutputInterface::DpmsMode::Suspend:
        return DrmOutput::DpmsMode::Suspend;
    case OutputInterface::DpmsMode::Off:
        return DrmOutput::DpmsMode::Off;
    default:
        Q_UNREACHABLE();
    }
}

static QHash<int, QByteArray> s_connectorNames = {
    {DRM_MODE_CONNECTOR_Unknown, QByteArrayLiteral("Unknown")},
    {DRM_MODE_CONNECTOR_VGA, QByteArrayLiteral("VGA")},
    {DRM_MODE_CONNECTOR_DVII, QByteArrayLiteral("DVI-I")},
    {DRM_MODE_CONNECTOR_DVID, QByteArrayLiteral("DVI-D")},
    {DRM_MODE_CONNECTOR_DVIA, QByteArrayLiteral("DVI-A")},
    {DRM_MODE_CONNECTOR_Composite, QByteArrayLiteral("Composite")},
    {DRM_MODE_CONNECTOR_SVIDEO, QByteArrayLiteral("SVIDEO")},
    {DRM_MODE_CONNECTOR_LVDS, QByteArrayLiteral("LVDS")},
    {DRM_MODE_CONNECTOR_Component, QByteArrayLiteral("Component")},
    {DRM_MODE_CONNECTOR_9PinDIN, QByteArrayLiteral("DIN")},
    {DRM_MODE_CONNECTOR_DisplayPort, QByteArrayLiteral("DP")},
    {DRM_MODE_CONNECTOR_HDMIA, QByteArrayLiteral("HDMI-A")},
    {DRM_MODE_CONNECTOR_HDMIB, QByteArrayLiteral("HDMI-B")},
    {DRM_MODE_CONNECTOR_TV, QByteArrayLiteral("TV")},
    {DRM_MODE_CONNECTOR_eDP, QByteArrayLiteral("eDP")},
    {DRM_MODE_CONNECTOR_VIRTUAL, QByteArrayLiteral("Virtual")},
    {DRM_MODE_CONNECTOR_DSI, QByteArrayLiteral("DSI")}
};


bool DrmOutput::init(drmModeConnector *connector)
{
    initEdid(connector);
    initDpms(connector);
    initUuid();
    if (m_backend->atomicModeSetting()) {
        if (!initPrimaryPlane()) {
            return false;
        }
    }
    m_savedCrtc.reset(drmModeGetCrtc(m_backend->fd(), m_crtcId));
    if (!blank()) {
        return false;
    }
    setDpms(DpmsMode::On);
    if (!m_waylandOutput.isNull()) {
        delete m_waylandOutput.data();
        m_waylandOutput.clear();
    }
    m_waylandOutput = waylandServer()->display()->createOutput();
    if (!m_waylandOutputDevice.isNull()) {
        delete m_waylandOutputDevice.data();
        m_waylandOutputDevice.clear();
    }
    m_waylandOutputDevice = waylandServer()->display()->createOutputDevice();
    m_waylandOutputDevice->setUuid(m_uuid);

    if (!m_edid.eisaId.isEmpty()) {
        m_waylandOutput->setManufacturer(QString::fromLatin1(m_edid.eisaId));
    } else {
        m_waylandOutput->setManufacturer(i18n("unknown"));
    }
    m_waylandOutputDevice->setManufacturer(m_waylandOutput->manufacturer());

    QString connectorName = s_connectorNames.value(connector->connector_type, QByteArrayLiteral("Unknown"));
    QString modelName;

    if (!m_edid.monitorName.isEmpty()) {
        QString model = QString::fromLatin1(m_edid.monitorName);
        if (!m_edid.serialNumber.isEmpty()) {
            model.append('/');
            model.append(QString::fromLatin1(m_edid.serialNumber));
        }
        modelName = model;
    } else if (!m_edid.serialNumber.isEmpty()) {
        modelName = QString::fromLatin1(m_edid.serialNumber);
    } else {
        modelName = i18n("unknown");
    }

    m_waylandOutput->setModel(connectorName + QStringLiteral("-") + QString::number(connector->connector_type_id) + QStringLiteral("-") + modelName);
    m_waylandOutputDevice->setModel(m_waylandOutput->model());
    
    QSize physicalSize = !m_edid.physicalSize.isEmpty() ? m_edid.physicalSize : QSize(connector->mmWidth, connector->mmHeight);
    // the size might be completely borked. E.g. Samsung SyncMaster 2494HS reports 160x90 while in truth it's 520x292
    // as this information is used to calculate DPI info, it's going to result in everything being huge
    const QByteArray unknown = QByteArrayLiteral("unkown");
    KConfigGroup group = kwinApp()->config()->group("EdidOverwrite").group(m_edid.eisaId.isEmpty() ? unknown : m_edid.eisaId)
                                                       .group(m_edid.monitorName.isEmpty() ? unknown : m_edid.monitorName)
                                                       .group(m_edid.serialNumber.isEmpty() ? unknown : m_edid.serialNumber);
    if (group.hasKey("PhysicalSize")) {
        const QSize overwriteSize = group.readEntry("PhysicalSize", physicalSize);
        qCWarning(KWIN_DRM) << "Overwriting monitor physical size for" << m_edid.eisaId << "/" << m_edid.monitorName << "/" << m_edid.serialNumber << " from " << physicalSize << "to " << overwriteSize;
        physicalSize = overwriteSize;
    }
    m_waylandOutput->setPhysicalSize(physicalSize);
    m_waylandOutputDevice->setPhysicalSize(physicalSize);

    // read in mode information
    for (int i = 0; i < connector->count_modes; ++i) {

        // TODO: in AMS here we could read and store for later every mode's blob_id
        // would simplify isCurrentMode(..) and presentAtomically(..) in case of mode set

        auto *m = &connector->modes[i];
        KWayland::Server::OutputInterface::ModeFlags flags;
        KWayland::Server::OutputDeviceInterface::ModeFlags deviceflags;
        if (isCurrentMode(m)) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Current;
            deviceflags |= KWayland::Server::OutputDeviceInterface::ModeFlag::Current;
        }
        if (m->type & DRM_MODE_TYPE_PREFERRED) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Preferred;
            deviceflags |= KWayland::Server::OutputDeviceInterface::ModeFlag::Preferred;
        }

        // Calculate higher precision (mHz) refresh rate
        // logic based on Weston, see compositor-drm.c
        quint64 refreshRate = (m->clock * 1000000LL / m->htotal + m->vtotal / 2) / m->vtotal;
        if (m->flags & DRM_MODE_FLAG_INTERLACE) {
            refreshRate *= 2;
        }
        if (m->flags & DRM_MODE_FLAG_DBLSCAN) {
            refreshRate /= 2;
        }
        if (m->vscan > 1) {
            refreshRate /= m->vscan;
        }
        m_waylandOutput->addMode(QSize(m->hdisplay, m->vdisplay), flags, refreshRate);

        KWayland::Server::OutputDeviceInterface::Mode mode;
        mode.id = i;
        mode.size = QSize(m->hdisplay, m->vdisplay);
        mode.flags = deviceflags;
        mode.refreshRate = refreshRate;
        qCDebug(KWIN_DRM) << "Adding mode: " << i << mode.size;
        m_waylandOutputDevice->addMode(mode);
    }

    // set dpms
    if (!m_dpms.isNull()) {
        m_waylandOutput->setDpmsSupported(true);
        m_waylandOutput->setDpmsMode(toWaylandDpmsMode(m_dpmsMode));
        connect(m_waylandOutput.data(), &KWayland::Server::OutputInterface::dpmsModeRequested, this,
            [this] (KWayland::Server::OutputInterface::DpmsMode mode) {
                setDpms(fromWaylandDpmsMode(mode));
            }, Qt::QueuedConnection
        );
    }

    m_waylandOutput->create();
    qCDebug(KWIN_DRM) << "Created OutputDevice";
    m_waylandOutputDevice->create();
    return true;
}

void DrmOutput::initUuid()
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(QByteArray::number(m_connector));
    hash.addData(m_edid.eisaId);
    hash.addData(m_edid.monitorName);
    hash.addData(m_edid.serialNumber);
    m_uuid = hash.result().toHex().left(10);
}

bool DrmOutput::isCurrentMode(const drmModeModeInfo *mode) const
{
    return mode->clock       == m_mode.clock
        && mode->hdisplay    == m_mode.hdisplay
        && mode->hsync_start == m_mode.hsync_start
        && mode->hsync_end   == m_mode.hsync_end
        && mode->htotal      == m_mode.htotal
        && mode->hskew       == m_mode.hskew
        && mode->vdisplay    == m_mode.vdisplay
        && mode->vsync_start == m_mode.vsync_start
        && mode->vsync_end   == m_mode.vsync_end
        && mode->vtotal      == m_mode.vtotal
        && mode->vscan       == m_mode.vscan
        && mode->vrefresh    == m_mode.vrefresh
        && mode->flags       == m_mode.flags
        && mode->type        == m_mode.type
        && qstrcmp(mode->name, m_mode.name) == 0;
}

static bool verifyEdidHeader(drmModePropertyBlobPtr edid)
{
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    if (data[0] != 0x00) {
        return false;
    }
    for (int i = 1; i < 7; ++i) {
        if (data[i] != 0xFF) {
            return false;
        }
    }
    if (data[7] != 0x00) {
        return false;
    }
    return true;
}

static QByteArray extractEisaId(drmModePropertyBlobPtr edid)
{
    /*
     * From EDID standard section 3.4:
     * The ID Manufacturer Name field, shown in Table 3.5, contains a 2-byte representation of the monitor's
     * manufacturer. This is the same as the EISA ID. It is based on compressed ASCII, “0001=A” ... “11010=Z”.
     *
     * The table:
     * | Byte |        Bit                    |
     * |      | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
     * ----------------------------------------
     * |  1   | 0)| (4| 3 | 2 | 1 | 0)| (4| 3 |
     * |      | * |    Character 1    | Char 2|
     * ----------------------------------------
     * |  2   | 2 | 1 | 0)| (4| 3 | 2 | 1 | 0)|
     * |      | Character2|      Character 3  |
     * ----------------------------------------
     **/
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    static const uint offset = 0x8;
    char id[4];
    if (data[offset] >> 7) {
        // bit at position 7 is not a 0
        return QByteArray();
    }
    // shift two bits to right, and with 7 right most bits
    id[0] = 'A' + ((data[offset] >> 2) & 0x1f) -1;
    // for first byte: take last two bits and shift them 3 to left (000xx000)
    // for second byte: shift 5 bits to right and take 3 right most bits (00000xxx)
    // or both together
    id[1] = 'A' + (((data[offset] & 0x3) << 3) | ((data[offset + 1] >> 5) & 0x7)) - 1;
    // take five right most bits
    id[2] = 'A' + (data[offset + 1] & 0x1f) - 1;
    id[3] = '\0';
    return QByteArray(id);
}

static void extractMonitorDescriptorDescription(drmModePropertyBlobPtr blob, DrmOutput::Edid &edid)
{
    // see section 3.10.3
    const uint8_t *data = reinterpret_cast<uint8_t*>(blob->data);
    static const uint offset = 0x36;
    static const uint blockLength = 18;
    for (int i = 0; i < 5; ++i) {
        const uint co = offset + i * blockLength;
        // Flag = 0000h when block used as descriptor
        if (data[co] != 0) {
            continue;
        }
        if (data[co + 1] != 0) {
            continue;
        }
        // Reserved = 00h when block used as descriptor
        if (data[co + 2] != 0) {
            continue;
        }
        /*
         * FFh: Monitor Serial Number - Stored as ASCII, code page # 437, ≤ 13 bytes.
         * FEh: ASCII String - Stored as ASCII, code page # 437, ≤ 13 bytes.
         * FDh: Monitor range limits, binary coded
         * FCh: Monitor name, stored as ASCII, code page # 437
         * FBh: Descriptor contains additional color point data
         * FAh: Descriptor contains additional Standard Timing Identifications
         * F9h - 11h: Currently undefined
         * 10h: Dummy descriptor, used to indicate that the descriptor space is unused
         * 0Fh - 00h: Descriptor defined by manufacturer.
         */
        if (data[co + 3] == 0xfc && edid.monitorName.isEmpty()) {
            edid.monitorName = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
        }
        if (data[co + 3] == 0xfe) {
            const QByteArray id = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
            if (!id.isEmpty()) {
                edid.eisaId = id;
            }
        }
        if (data[co + 3] == 0xff) {
            edid.serialNumber = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
        }
    }
}

static QByteArray extractSerialNumber(drmModePropertyBlobPtr edid)
{
    // see section 3.4
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    static const uint offset = 0x0C;
    /*
     * The ID serial number is a 32-bit serial number used to differentiate between individual instances of the same model
     * of monitor. Its use is optional. When used, the bit order for this field follows that shown in Table 3.6. The EDID
     * structure Version 1 Revision 1 and later offer a way to represent the serial number of the monitor as an ASCII string
     * in a separate descriptor block.
     */
    uint32_t serialNumber = 0;
    serialNumber  = (uint32_t) data[offset + 0];
    serialNumber |= (uint32_t) data[offset + 1] << 8;
    serialNumber |= (uint32_t) data[offset + 2] << 16;
    serialNumber |= (uint32_t) data[offset + 3] << 24;
    if (serialNumber == 0) {
        return QByteArray();
    }
    return QByteArray::number(serialNumber);
}

static QSize extractPhysicalSize(drmModePropertyBlobPtr edid)
{
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    return QSize(data[0x15], data[0x16]) * 10;
}

void DrmOutput::initEdid(drmModeConnector *connector)
{
    ScopedDrmPointer<_drmModePropertyBlob, &drmModeFreePropertyBlob> edid;
    for (int i = 0; i < connector->count_props; ++i) {
        ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> property(drmModeGetProperty(m_backend->fd(), connector->props[i]));
        if (!property) {
            continue;
        }
        if ((property->flags & DRM_MODE_PROP_BLOB) && qstrcmp(property->name, "EDID") == 0) {
            edid.reset(drmModeGetPropertyBlob(m_backend->fd(), connector->prop_values[i]));
        }
    }
    if (!edid) {
        return;
    }

    // for documentation see: http://read.pudn.com/downloads110/ebook/456020/E-EDID%20Standard.pdf
    if (edid->length < 128) {
        return;
    }
    if (!verifyEdidHeader(edid.data())) {
        return;
    }
    m_edid.eisaId = extractEisaId(edid.data());
    m_edid.serialNumber = extractSerialNumber(edid.data());

    // parse monitor descriptor description
    extractMonitorDescriptorDescription(edid.data(), m_edid);

    m_edid.physicalSize = extractPhysicalSize(edid.data());
}

bool DrmOutput::initPrimaryPlane()
{
    for (int i = 0; i < m_backend->planes().size(); ++i) {
        DrmPlane* p = m_backend->planes()[i];
        if (!p) {
            continue;
        }
        if (p->type() != DrmPlane::TypeIndex::Primary) {
            continue;
        }
        if (p->output()) {     // Plane already has an output
            continue;
        }
        if (m_primaryPlane) {     // Output already has a primary plane
            continue;
        }
        if (!p->isCrtcSupported(m_crtcId)) {
            continue;
        }
        p->setOutput(this);
        m_primaryPlane = p;
        qCDebug(KWIN_DRM) << "Initialized primary plane" << p->id() << "on CRTC" << m_crtcId;
        return true;
    }
    qCCritical(KWIN_DRM) << "Failed to initialize primary plane.";
    return false;
}

bool DrmOutput::initCursorPlane()       // TODO: Add call in init (but needs layer support in general first)
{
    for (int i = 0; i < m_backend->planes().size(); ++i) {
        DrmPlane* p = m_backend->planes()[i];
        if (!p) {
            continue;
        }
        if (p->type() != DrmPlane::TypeIndex::Cursor) {
            continue;
        }
        if (p->output()) {     // Plane already has an output
            continue;
        }
        if (m_cursorPlane) {     // Output already has a cursor plane
            continue;
        }
        if (!p->isCrtcSupported(m_crtcId)) {
            continue;
        }
        p->setOutput(this);
        m_cursorPlane = p;
        qCDebug(KWIN_DRM) << "Initialized cursor plane" << p->id() << "on CRTC" << m_crtcId;
        return true;
    }
    return false;
}

void DrmOutput::initDpms(drmModeConnector *connector)
{
    for (int i = 0; i < connector->count_props; ++i) {
        ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> property(drmModeGetProperty(m_backend->fd(), connector->props[i]));
        if (!property) {
            continue;
        }
        if (qstrcmp(property->name, "DPMS") == 0) {
            m_dpms.swap(property);
            break;
        }
    }
}

void DrmOutput::setDpms(DrmOutput::DpmsMode mode)
{
    if (m_dpms.isNull()) {
        return;
    }
    if (mode == m_dpmsMode) {
        qCDebug(KWIN_DRM) << "New DPMS mode equals old mode. DPMS unchanged.";
        return;
    }

    if (m_backend->atomicModeSetting()) {
        drmModeAtomicReq *req = drmModeAtomicAlloc();

        if (atomicReqModesetPopulate(req, mode == DpmsMode::On) == DrmObject::AtomicReturn::Error) {
            qCWarning(KWIN_DRM) << "Failed to populate atomic request for output" << m_crtcId;
            return;
        }
        if (drmModeAtomicCommit(m_backend->fd(), req, DRM_MODE_ATOMIC_ALLOW_MODESET, this)) {
            qCWarning(KWIN_DRM) << "Failed to commit atomic request for output" << m_crtcId;
        } else {
            qCDebug(KWIN_DRM) << "DPMS set for output" << m_crtcId;
        }
        drmModeAtomicFree(req);
    } else {
        if (drmModeConnectorSetProperty(m_backend->fd(), m_connector, m_dpms->prop_id, uint64_t(mode)) < 0) {
            qCWarning(KWIN_DRM) << "Setting DPMS failed";
            return;
        }
    }

    m_dpmsMode = mode;
    if (m_waylandOutput) {
        m_waylandOutput->setDpmsMode(toWaylandDpmsMode(m_dpmsMode));
    }
    emit dpmsChanged();
    if (m_dpmsMode != DpmsMode::On) {
        m_backend->outputWentOff();
    } else {
        m_backend->checkOutputsAreOn();
        blank();
        if (Compositor *compositor = Compositor::self()) {
            compositor->addRepaintFull();
        }
    }
}

QString DrmOutput::name() const
{
    if (!m_waylandOutput) {
        return i18n("unknown");
    }
    return QStringLiteral("%1 %2").arg(m_waylandOutput->manufacturer()).arg(m_waylandOutput->model());
}

int DrmOutput::currentRefreshRate() const
{
    if (!m_waylandOutput) {
        return 60000;
    }
    return m_waylandOutput->refreshRate();
}

void DrmOutput::setGlobalPos(const QPoint &pos)
{
    m_globalPos = pos;
    if (m_waylandOutput) {
        m_waylandOutput->setGlobalPosition(pos);
    }
    if (m_waylandOutputDevice) {
        m_waylandOutputDevice->setGlobalPosition(pos);
    }
}

void DrmOutput::setChanges(KWayland::Server::OutputChangeSet *changes)
{
    m_changeset = changes;
    qCDebug(KWIN_DRM) << "set changes in DrmOutput";
    commitChanges();
}

bool DrmOutput::commitChanges()
{
    Q_ASSERT(!m_waylandOutputDevice.isNull());
    Q_ASSERT(!m_waylandOutput.isNull());

    if (m_changeset.isNull()) {
        qCDebug(KWIN_DRM) << "no changes";
        // No changes to an output is an entirely valid thing
        return true;
    }

    if (m_changeset->enabledChanged()) {
        qCDebug(KWIN_DRM) << "Setting enabled:";
        m_waylandOutputDevice->setEnabled(m_changeset->enabled());
    }
    if (m_changeset->modeChanged()) {
        qCDebug(KWIN_DRM) << "Setting new mode:" << m_changeset->mode();
        m_waylandOutputDevice->setCurrentMode(m_changeset->mode());
        // FIXME: implement for wl_output
    }
    if (m_changeset->transformChanged()) {
        qCDebug(KWIN_DRM) << "Server setting transform: " << (int)(m_changeset->transform());
        m_waylandOutputDevice->setTransform(m_changeset->transform());
        // FIXME: implement for wl_output
    }
    if (m_changeset->positionChanged()) {
        qCDebug(KWIN_DRM) << "Server setting position: " << m_changeset->position();
        m_waylandOutput->setGlobalPosition(m_changeset->position());
        m_waylandOutputDevice->setGlobalPosition(m_changeset->position());
        setGlobalPos(m_changeset->position());
        // may just work already!
    }
    if (m_changeset->scaleChanged()) {
        qCDebug(KWIN_DRM) << "Setting scale:" << m_changeset->scale();
        m_waylandOutputDevice->setScale(m_changeset->scale());
        // FIXME: implement for wl_output
    }
    return true;
}

void DrmOutput::pageFlipped()
{
    if (m_backend->atomicModeSetting()){
        foreach (DrmPlane *p, m_planesFlipList) {
            pageFlippedBufferRemover(p->current(), p->next());
            p->setCurrent(p->next());
            p->setNext(nullptr);
        }
        m_planesFlipList.clear();

    } else {
        if (!m_nextBuffer) {
            return;
        }
        pageFlippedBufferRemover(m_currentBuffer, m_nextBuffer);
        m_currentBuffer = m_nextBuffer;
        m_nextBuffer = nullptr;
    }
    cleanupBlackBuffer();
}

void DrmOutput::pageFlippedBufferRemover(DrmBuffer *oldbuffer, DrmBuffer *newbuffer)
{
    if (newbuffer->deleteAfterPageFlip()) {
        if ( oldbuffer && oldbuffer != newbuffer ) {
            delete oldbuffer;
        }
    } else {
        // although oldbuffer's pointer is remapped in pageFlipped(),
        // we ignore the pointer completely anywhere else in this case
        newbuffer->releaseGbm();
    }
}

void DrmOutput::cleanupBlackBuffer()
{
    if (m_blackBuffer) {
        delete m_blackBuffer;
        m_blackBuffer = nullptr;
    }
}

bool DrmOutput::blank()
{
    if (!m_blackBuffer) {
        m_blackBuffer = m_backend->createBuffer(size());
        if (!m_blackBuffer->map()) {
            cleanupBlackBuffer();
            return false;
        }
        m_blackBuffer->image()->fill(Qt::black);
    }
    // TODO: Do this atomically
    return setModeLegacy(m_blackBuffer);
}

bool DrmOutput::present(DrmBuffer *buffer)
{
    if (!buffer || buffer->bufferId() == 0) {
            return false;
    }
    if (m_backend->atomicModeSetting()) {
        return presentAtomically(buffer);
    } else {
        return presentLegacy(buffer);
    }
}

bool DrmOutput::presentAtomically(DrmBuffer *buffer)
{
    if (!LogindIntegration::self()->isActiveSession()) {
        qCWarning(KWIN_DRM) << "Logind session not active.";
        return false;
    }
    if (m_dpmsMode != DpmsMode::On) {
        qCWarning(KWIN_DRM) << "No present() while screen off.";
        return false;
    }
    if (m_primaryPlane->next()) {
        qCWarning(KWIN_DRM) << "Page not yet flipped.";
        return false;
    }

    DrmObject::AtomicReturn ret;
    uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT;

    // TODO: throwing an exception would be really handy here! (would mean change of compile options)
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    if (!req) {
            qCWarning(KWIN_DRM) << "DRM: couldn't allocate atomic request";
            delete buffer;
            return false;
    }

    // Do we need to set a new mode first?
    bool doModeset = !m_primaryPlane->current();
    if (doModeset) {
        qCDebug(KWIN_DRM) << "Atomic Modeset requested";

        if (drmModeCreatePropertyBlob(m_backend->fd(), &m_mode, sizeof(m_mode), &m_blobId)) {
            qCWarning(KWIN_DRM) << "Failed to create property blob";
            delete buffer;
            return false;
        }

        ret = atomicReqModesetPopulate(req, true);
        if (ret == DrmObject::AtomicReturn::Error){
            drmModeAtomicFree(req);
            delete buffer;
            return false;
        }
        if (ret == DrmObject::AtomicReturn::Success) {
            flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
        }
    }

    m_primaryPlane->setNext(buffer);            // TODO: Later not only use the primary plane for the buffer!
                                                // i.e.: Assign planes
    bool anyDamage = false;
    foreach (DrmPlane* p, m_backend->planes()){
        if (p->output() != this) {
            continue;
        }
        ret = p->atomicReqPlanePopulate(req);
        if (ret == DrmObject::AtomicReturn::Error) {
            drmModeAtomicFree(req);
            m_primaryPlane->setNext(nullptr);
            m_planesFlipList.clear();
            delete buffer;
            return false;
        }
        if (ret == DrmObject::AtomicReturn::Success) {
            anyDamage = true;
            m_planesFlipList << p;
        }
    }

    // no damage but force flip for atleast the primary plane anyway
    if (!anyDamage) {
        m_primaryPlane->setPropsValid(0);
        if (m_primaryPlane->atomicReqPlanePopulate(req) == DrmObject::AtomicReturn::Error) {
            drmModeAtomicFree(req);
            m_primaryPlane->setNext(nullptr);
            m_planesFlipList.clear();
            delete buffer;
            return false;
        }
        m_planesFlipList << m_primaryPlane;
    }

    if (drmModeAtomicCommit(m_backend->fd(), req, flags, this)) {
        qCWarning(KWIN_DRM) << "Atomic request failed to commit:" << strerror(errno);
        drmModeAtomicFree(req);
        m_primaryPlane->setNext(nullptr);
        m_planesFlipList.clear();
        delete buffer;
        return false;
    }

    if (doModeset) {
        m_crtc->setPropsValid(m_crtc->propsValid() | m_crtc->propsPending());
        m_conn->setPropsValid(m_conn->propsValid() | m_conn->propsPending());
    }
    foreach (DrmPlane* p, m_planesFlipList) {
        p->setPropsValid(p->propsValid() | p->propsPending());
    }

    drmModeAtomicFree(req);
    return true;
}


bool DrmOutput::presentLegacy(DrmBuffer *buffer)
{
    if (m_nextBuffer) {
        return false;
    }
    if (!LogindIntegration::self()->isActiveSession()) {
        m_nextBuffer = buffer;
        return false;
    }
    if (m_dpmsMode != DpmsMode::On) {
        return false;
    }

    // Do we need to set a new mode first?
    if (m_lastStride != buffer->stride() || m_lastGbm != buffer->isGbm()){
        if (!setModeLegacy(buffer))
            return false;
    }
    int errno_save = 0;
    const bool ok = drmModePageFlip(m_backend->fd(), m_crtcId, buffer->bufferId(), DRM_MODE_PAGE_FLIP_EVENT, this) == 0;
    if (ok) {
        m_nextBuffer = buffer;
    } else {
        errno_save = errno;
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno);
        delete buffer;
    }
    return ok;
}

bool DrmOutput::setModeLegacy(DrmBuffer *buffer)
{
    if (drmModeSetCrtc(m_backend->fd(), m_crtcId, buffer->bufferId(), 0, 0, &m_connector, 1, &m_mode) == 0) {
        m_lastStride = buffer->stride();
        m_lastGbm = buffer->isGbm();
        return true;
    } else {
        qCWarning(KWIN_DRM) << "Mode setting failed";
        return false;
    }
}

DrmObject::AtomicReturn DrmOutput::atomicReqModesetPopulate(drmModeAtomicReq *req, bool enable)
{
    if (enable) {
        m_primaryPlane->setPropValue(int(DrmPlane::PropertyIndex::SrcW), m_mode.hdisplay << 16);
        m_primaryPlane->setPropValue(int(DrmPlane::PropertyIndex::SrcH), m_mode.vdisplay << 16);
        m_primaryPlane->setPropValue(int(DrmPlane::PropertyIndex::CrtcW), m_mode.hdisplay);
        m_primaryPlane->setPropValue(int(DrmPlane::PropertyIndex::CrtcH), m_mode.vdisplay);
    } else {
        m_primaryPlane->setPropValue(int(DrmPlane::PropertyIndex::SrcW), 0);
        m_primaryPlane->setPropValue(int(DrmPlane::PropertyIndex::SrcH), 0);
        m_primaryPlane->setPropValue(int(DrmPlane::PropertyIndex::CrtcW), 0);
        m_primaryPlane->setPropValue(int(DrmPlane::PropertyIndex::CrtcH), 0);
    }

    bool ret = true;

    m_crtc->setPropsPending(0);
    m_conn->setPropsPending(0);

    ret &= m_conn->atomicAddProperty(req, int(DrmConnector::PropertyIndex::CrtcId), enable ? m_crtc->id() : 0);
    ret &= m_crtc->atomicAddProperty(req, int(DrmCrtc::PropertyIndex::ModeId), enable ? m_blobId : 0);
    ret &= m_crtc->atomicAddProperty(req, int(DrmCrtc::PropertyIndex::Active), enable);

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic modeset";
        return DrmObject::AtomicReturn::Error;
    }
    if (!m_crtc->propsPending() && !m_conn->propsPending()) {
        return DrmObject::AtomicReturn::NoChange;
    }
    return DrmObject::AtomicReturn::Success;
}

}
