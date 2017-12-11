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
#ifndef KWIN_DRM_BACKEND_H
#define KWIN_DRM_BACKEND_H
#include "platform.h"
#include "input.h"

#include "drm_buffer.h"
#if HAVE_GBM
#include "drm_buffer_gbm.h"
#endif
#include "drm_inputeventfilter.h"
#include "drm_pointer.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QSize>
#include <xf86drmMode.h>

#include <memory>

struct gbm_bo;
struct gbm_device;
struct gbm_surface;

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

namespace ColorCorrect {
struct GammaRamp;
}

class Udev;
class UdevMonitor;

class DrmOutput;
class DrmPlane;
class DrmCrtc;
class DrmConnector;
class GbmSurface;


class KWIN_EXPORT DrmBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "drm.json")
public:
    explicit DrmBackend(QObject *parent = nullptr);
    virtual ~DrmBackend();

    void configurationChangeRequested(KWayland::Server::OutputConfigurationInterface *config) override;
    Screens *createScreens(QObject *parent = nullptr) override;
    QPainterBackend *createQPainterBackend() override;
    OpenGLBackend* createOpenGLBackend() override;

    void init() override;
    DrmDumbBuffer *createBuffer(const QSize &size);
#if HAVE_GBM
    DrmSurfaceBuffer *createBuffer(const std::shared_ptr<GbmSurface> &surface);
#endif
    void present(DrmBuffer *buffer, DrmOutput *output);

    int fd() const {
        return m_fd;
    }
    QVector<DrmOutput*> outputs() const {
        return m_outputs;
    }
    QVector<DrmOutput*> enabledOutputs() const {
        return m_enabledOutputs;
    }
    QVector<DrmPlane*> planes() const {
        return m_planes;
    }
    QVector<DrmPlane*> overlayPlanes() const {
        return m_overlayPlanes;
    }

    void outputWentOff();
    void checkOutputsAreOn();

    // QPainter reuses buffers
    bool deleteBufferAfterPageFlip() const {
        return m_deleteBufferAfterPageFlip;
    }
    // returns use of AMS, default is not/legacy
    bool atomicModeSetting() const {
        return m_atomicModeSetting;
    }

    void setGbmDevice(gbm_device *device) {
        m_gbmDevice = device;
    }
    gbm_device *gbmDevice() const {
        return m_gbmDevice;
    }
    int gammaRampSize(int screen) const override;
    bool setGammaRamp(int screen, ColorCorrect::GammaRamp &gamma) override;

    QVector<CompositingType> supportedCompositors() const override;

    QString supportInformation() const override;

public Q_SLOTS:
    void turnOutputsOn();

Q_SIGNALS:
    /**
     * Emitted whenever an output is removed/disabled
     */
    void outputRemoved(KWin::DrmOutput *output);
    /**
     * Emitted whenever an output is added/enabled
     */
    void outputAdded(KWin::DrmOutput *output);

protected:

    void doHideCursor() override;
    void doShowCursor() override;

private:
    static void pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
    void openDrm();
    void activate(bool active);
    void reactivate();
    void deactivate();
    void updateOutputs();
    void setCursor();
    void updateCursor();
    void moveCursor();
    void initCursor();
    void outputDpmsChanged();
    void readOutputsConfiguration();
    QByteArray generateOutputConfigurationUuid() const;
    DrmOutput *findOutput(quint32 connector);
    DrmOutput *findOutput(const QByteArray &uuid);
    QScopedPointer<Udev> m_udev;
    QScopedPointer<UdevMonitor> m_udevMonitor;
    int m_fd = -1;
    int m_drmId = 0;
    // all crtcs
    QVector<DrmCrtc*> m_crtcs;
    // all connectors
    QVector<DrmConnector*> m_connectors;
    // active output pipelines (planes + crtc + encoder + connector)
    QVector<DrmOutput*> m_outputs;
    // active and enabled pipelines (above + wl_output)
    QVector<DrmOutput*> m_enabledOutputs;

    bool m_deleteBufferAfterPageFlip;
    bool m_atomicModeSetting = false;
    bool m_cursorEnabled = false;
    QSize m_cursorSize;
    int m_pageFlipsPending = 0;
    bool m_active = false;
    // all available planes: primarys, cursors and overlays
    QVector<DrmPlane*> m_planes;
    QVector<DrmPlane*> m_overlayPlanes;
    QScopedPointer<DpmsInputEventFilter> m_dpmsFilter;
    KWayland::Server::OutputManagementInterface *m_outputManagement = nullptr;
    gbm_device *m_gbmDevice = nullptr;
};


}

#endif

