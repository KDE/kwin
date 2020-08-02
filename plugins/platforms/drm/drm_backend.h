/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
#include <QVector>
#include <xf86drmMode.h>

#include <memory>

struct gbm_bo;
struct gbm_device;
struct gbm_surface;

namespace KWin
{

class Udev;
class UdevMonitor;

class DrmOutput;
class DrmPlane;
class DrmCrtc;
class DrmConnector;
class GbmSurface;
class Cursor;

class KWIN_EXPORT DrmBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "drm.json")
public:
    explicit DrmBackend(QObject *parent = nullptr);
    ~DrmBackend() override;

    Screens *createScreens(QObject *parent = nullptr) override;
    QPainterBackend *createQPainterBackend() override;
    OpenGLBackend* createOpenGLBackend() override;
    DmaBufTexture *createDmaBufTexture(const QSize &size) override;

    void init() override;
    void prepareShutdown() override;

    DrmDumbBuffer *createBuffer(const QSize &size);
#if HAVE_GBM
    DrmSurfaceBuffer *createBuffer(const std::shared_ptr<GbmSurface> &surface);
#endif
    bool present(DrmBuffer *buffer, DrmOutput *output);

    int fd() const {
        return m_fd;
    }
    Outputs outputs() const override;
    Outputs enabledOutputs() const override;
    QVector<DrmOutput*> drmOutputs() const {
        return m_outputs;
    }
    QVector<DrmOutput*> drmEnabledOutputs() const {
        return m_enabledOutputs;
    }

    void enableOutput(DrmOutput *output, bool enable);

    QVector<DrmPlane*> planes() const {
        return m_planes;
    }
    QVector<DrmPlane*> overlayPlanes() const {
        return m_overlayPlanes;
    }

    void createDpmsFilter();
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

    QByteArray devNode() const {
        return m_devNode;
    }

#if HAVE_EGL_STREAMS
    bool useEglStreams() const {
        return m_useEglStreams;
    }
#endif

    QVector<CompositingType> supportedCompositors() const override;

    QString supportInformation() const override;

    bool isCursorEnabled() const {
        return m_cursorEnabled;
    };

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
    bool updateOutputs();
    void setCursor();
    void updateCursor();
    void moveCursor(Cursor *cursor, const QPoint &pos);
    void initCursor();
    void readOutputsConfiguration();
    void writeOutputsConfiguration();
    QByteArray generateOutputConfigurationUuid() const;
    DrmOutput *findOutput(quint32 connector);
    void updateOutputsEnabled();
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
    QByteArray m_devNode;
#if HAVE_EGL_STREAMS
    bool m_useEglStreams = false;
#endif
    // all available planes: primarys, cursors and overlays
    QVector<DrmPlane*> m_planes;
    QVector<DrmPlane*> m_overlayPlanes;
    QScopedPointer<DpmsInputEventFilter> m_dpmsFilter;
    gbm_device *m_gbmDevice = nullptr;
};


}

#endif

