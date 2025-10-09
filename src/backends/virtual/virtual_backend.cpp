/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_backend.h"

#include "core/drmdevice.h"
#include "virtual_egl_backend.h"
#include "virtual_output.h"
#include "virtual_qpainter_backend.h"

#include <QDir>
#include <QFile>

#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>

namespace KWin
{

static drmDevicePtr openDevice(const QString &filePath)
{
    const int fd = open(filePath.toUtf8().constData(), O_CLOEXEC);
    if (fd == -1) {
        qDebug() << "open() failed:" << strerror(errno);
        return nullptr;
    }

    drmDevicePtr device;
    if (const int error = drmGetDevice(fd, &device)) {
        qDebug() << "drmGetDevice() failed:" << strerror(errno);
        qDebug() << "drmGetDevice() failed:" << strerror(-error);
        return nullptr;
    }

    return device;
}

static std::unique_ptr<DrmDevice> findRenderDevice()
{
    qDebug() << "/dev/dri/card0 exists:" << QFile::exists(QStringLiteral("/dev/dri/card0"));
    qDebug() << "/dev/dri/card1 exists:" << QFile::exists(QStringLiteral("/dev/dri/card1"));
    qDebug() << "/dev/dri/card2 exists:" << QFile::exists(QStringLiteral("/dev/dri/card2"));

    qDebug() << "/dev/dri/renderD128 exists:" << QFile::exists(QStringLiteral("/dev/dri/renderD128"));
    qDebug() << "/dev/dri/renderD129 exists:" << QFile::exists(QStringLiteral("/dev/dri/renderD129"));

    qDebug() << "/dev/dri contents:" << QDir(QStringLiteral("/dev/dri")).entryList(QDir::AllEntries | QDir::System);
    qDebug() << "/dev/dri/by-path contents:" << QDir(QStringLiteral("/dev/dri/by-path")).entryList(QDir::AllEntries | QDir::System);

    qDebug() << "/dev/dri/card1 subsystem:" << QFileInfo(QStringLiteral("/sys/dev/char/226:1/device/subsystem")).symLinkTarget();
    qDebug() << "/dev/dri/card2 subsystem:" << QFileInfo(QStringLiteral("/sys/dev/char/226:2/device/subsystem")).symLinkTarget();
    qDebug() << "/dev/dri/renderD128 subsystem:" << QFileInfo(QStringLiteral("/sys/dev/char/226:128/device/subsystem")).symLinkTarget();

    drmDevicePtr card1 = openDevice(QStringLiteral("/dev/dri/card1"));
    qDebug() << "card1 opened:" << bool(card1);
    drmDevicePtr card2 = openDevice(QStringLiteral("/dev/dri/card2"));
    qDebug() << "card2 opened:" << bool(card2);
    drmDevicePtr renderD128 = openDevice(QStringLiteral("/dev/dri/renderD128"));
    qDebug() << "renderD128 opened:" << bool(renderD128);

    qDebug() << "card1 == card2" << drmDevicesEqual(card1, card2);
    qDebug() << "card1 == renderD128" << drmDevicesEqual(card1, renderD128);
    qDebug() << "renderD128 == card2" << drmDevicesEqual(renderD128, card2);

    const int deviceCount = drmGetDevices2(0, nullptr, 0);
    if (deviceCount <= 0) {
        return nullptr;
    }

    QList<drmDevice *> devices(deviceCount);
    if (drmGetDevices2(0, devices.data(), devices.size()) < 0) {
        return nullptr;
    }
    auto deviceCleanup = qScopeGuard([&devices]() {
        drmFreeDevices(devices.data(), devices.size());
    });

    qDebug() << "num. available devices:" << deviceCount;

    for (drmDevice *device : std::as_const(devices)) {
        // If it's a vgem device, prefer the primary node because gbm will attempt to allocate
        // dumb buffers and they can be allocated only on the primary node.
        int nodeType = DRM_NODE_RENDER;
        if (device->bustype == DRM_BUS_PLATFORM) {
            qDebug() << "platform fullname:" << device->businfo.platform->fullname;
            if (strcmp(device->businfo.platform->fullname, "vgem") == 0) {
                nodeType = DRM_NODE_PRIMARY;
            }
        }

        qDebug() << "bus type:" << device->bustype << "available nodes:" << device->available_nodes << "nodeType:" << nodeType;

        if (device->available_nodes & (1 << nodeType)) {
            qDebug() << device->nodes[nodeType];
            if (auto ret = DrmDevice::open(device->nodes[nodeType])) {
                return ret;
            }
        }
    }

    return nullptr;
}

VirtualBackend::VirtualBackend(QObject *parent)
    : OutputBackend(parent)
    , m_drmDevice(findRenderDevice())
{
}

VirtualBackend::~VirtualBackend()
{
}

bool VirtualBackend::initialize()
{
    return true;
}

QList<CompositingType> VirtualBackend::supportedCompositors() const
{
    QList<CompositingType> compositingTypes;
    if (m_drmDevice) {
        compositingTypes.append(OpenGLCompositing);
    }
    compositingTypes.append(QPainterCompositing);
    return compositingTypes;
}

DrmDevice *VirtualBackend::drmDevice() const
{
    return m_drmDevice.get();
}

std::unique_ptr<QPainterBackend> VirtualBackend::createQPainterBackend()
{
    return std::make_unique<VirtualQPainterBackend>(this);
}

std::unique_ptr<EglBackend> VirtualBackend::createOpenGLBackend()
{
    return std::make_unique<VirtualEglBackend>(this);
}

Outputs VirtualBackend::outputs() const
{
    return m_outputs;
}

VirtualOutput *VirtualBackend::createOutput(const OutputInfo &info)
{
    VirtualOutput *output = new VirtualOutput(this, info.internal, info.physicalSizeInMM, info.panelOrientation, info.edid, info.edidIdentifierOverride, info.connectorName, info.mstPath);
    output->init(info.geometry.topLeft(), info.geometry.size() * info.scale, info.scale, info.modes);
    m_outputs.append(output);
    Q_EMIT outputAdded(output);
    return output;
}

Output *VirtualBackend::addOutput(const OutputInfo &info)
{
    VirtualOutput *output = createOutput(info);
    Q_EMIT outputsQueried();
    return output;
}

void VirtualBackend::setVirtualOutputs(const QList<OutputInfo> &infos)
{
    const QList<VirtualOutput *> removed = m_outputs;

    for (const auto &info : infos) {
        createOutput(info);
    }

    for (VirtualOutput *output : removed) {
        m_outputs.removeOne(output);
        Q_EMIT outputRemoved(output);
        output->unref();
    }

    Q_EMIT outputsQueried();
}

void VirtualBackend::setEglDisplay(std::unique_ptr<EglDisplay> &&display)
{
    m_display = std::move(display);
}

EglDisplay *VirtualBackend::sceneEglDisplayObject() const
{
    return m_display.get();
}

} // namespace KWin

#include "moc_virtual_backend.cpp"
