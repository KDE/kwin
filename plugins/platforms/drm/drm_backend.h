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
#include "drm_inputeventfilter.h"
#include "drm_pointer.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QSize>
#include <xf86drmMode.h>

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

class Udev;
class UdevMonitor;

class DrmOutput;
class DrmPlane;


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
    DrmBuffer *createBuffer(const QSize &size);
    DrmBuffer *createBuffer(gbm_surface *surface);
    void present(DrmBuffer *buffer, DrmOutput *output);

    QSize size() const;
    int fd() const {
        return m_fd;
    }
    QVector<DrmOutput*> outputs() const {
        return m_outputs;
    }
    QVector<DrmBuffer*> buffers() const {
        return m_buffers;
    }
    QVector<DrmPlane*> planes() const {
        return m_planes;
    }
    void bufferDestroyed(DrmBuffer *b);

    void outputWentOff();
    void checkOutputsAreOn();

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

public Q_SLOTS:
    void turnOutputsOn();

Q_SIGNALS:
    void outputRemoved(KWin::DrmOutput *output);
    void outputAdded(KWin::DrmOutput *output);

private:
    static void pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
    void openDrm();
    void activate(bool active);
    void reactivate();
    void deactivate();
    void queryResources();
    void setCursor();
    void updateCursor();
    void hideCursor();
    void moveCursor();
    void initCursor();
    quint32 findCrtc(drmModeRes *res, drmModeConnector *connector, bool *ok = nullptr);
    bool crtcIsUsed(quint32 crtc);
    void outputDpmsChanged();
    void readOutputsConfiguration();
    QByteArray generateOutputConfigurationUuid() const;
    DrmOutput *findOutput(quint32 connector);
    DrmOutput *findOutput(const QByteArray &uuid);
    QScopedPointer<Udev> m_udev;
    QScopedPointer<UdevMonitor> m_udevMonitor;
    int m_fd = -1;
    int m_drmId = 0;
    QVector<DrmOutput*> m_outputs;
    DrmBuffer *m_cursor[2];
    bool m_atomicModeSetting = false;
    bool m_cursorEnabled = false;
    int m_cursorIndex = 0;
    int m_pageFlipsPending = 0;
    bool m_active = false;
    QVector<DrmBuffer*> m_buffers;
    // all available planes: primarys, cursors and overlays
    QVector<DrmPlane*> m_planes;
    QScopedPointer<DpmsInputEventFilter> m_dpmsFilter;
    KWayland::Server::OutputManagementInterface *m_outputManagement = nullptr;
    gbm_device *m_gbmDevice = nullptr;
};


}

#endif

