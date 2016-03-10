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
#include "abstract_backend.h"
#include "input.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QSize>
#include <xf86drmMode.h>

struct gbm_bo;
struct gbm_surface;

namespace KWayland
{
namespace Server
{
class OutputInterface;
class OutputDeviceInterface;
class OutputChangeSet;
}
}

namespace KWin
{

class Udev;
class UdevMonitor;

class DrmBuffer;
class DrmOutput;
class DpmsInputEventFilter;

template <typename Pointer, void (*cleanupFunc)(Pointer*)>
struct DrmCleanup
{
    static inline void cleanup(Pointer *ptr)
    {
        cleanupFunc(ptr);
    }
};
template <typename T, void (*cleanupFunc)(T*)> using ScopedDrmPointer = QScopedPointer<T, DrmCleanup<T, cleanupFunc>>;

class KWIN_EXPORT DrmBackend : public AbstractBackend
{
    Q_OBJECT
    Q_INTERFACES(KWin::AbstractBackend)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.AbstractBackend" FILE "drm.json")
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
    void bufferDestroyed(DrmBuffer *b);

    void outputWentOff();
    void checkOutputsAreOn();

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
    int m_cursorIndex = 0;
    int m_pageFlipsPending = 0;
    bool m_active = false;
    QVector<DrmBuffer*> m_buffers;
    QScopedPointer<DpmsInputEventFilter> m_dpmsFilter;
};

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
    void showCursor(DrmBuffer *buffer);
    void hideCursor();
    void moveCursor(const QPoint &globalPos);
    bool present(DrmBuffer *buffer);
    void pageFlipped();
    void init(drmModeConnector *connector);
    void restoreSaved();
    void blank();

    /**
     * This sets the changes and tests them against the DRM output
     */
    void setChanges(KWayland::Server::OutputChangeSet *changeset);
    bool commitChanges();

    QSize size() const;
    QRect geometry() const;
    QString name() const;
    int currentRefreshRate() const;
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
    bool setMode(DrmBuffer *buffer);
    void initEdid(drmModeConnector *connector);
    void initDpms(drmModeConnector *connector);
    bool isCurrentMode(const drmModeModeInfo *mode) const;
    void initUuid();
    void setGlobalPos(const QPoint &pos);

    DrmBackend *m_backend;
    QPoint m_globalPos;
    quint32 m_crtcId = 0;
    quint32 m_connector = 0;
    quint32 m_lastStride = 0;
    bool m_lastGbm = false;
    drmModeModeInfo m_mode;
    DrmBuffer *m_currentBuffer = nullptr;
    DrmBuffer *m_blackBuffer = nullptr;
    struct CrtcCleanup {
        static void inline cleanup(_drmModeCrtc *ptr) {
            drmModeFreeCrtc(ptr);
        }
    };
    Edid m_edid;
    QScopedPointer<_drmModeCrtc, CrtcCleanup> m_savedCrtc;
    QPointer<KWayland::Server::OutputInterface> m_waylandOutput;
    QPointer<KWayland::Server::OutputDeviceInterface> m_waylandOutputDevice;
    QPointer<KWayland::Server::OutputChangeSet> m_changeset;
    ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> m_dpms;
    DpmsMode m_dpmsMode = DpmsMode::On;
    QByteArray m_uuid;
};

class DrmBuffer
{
public:
    ~DrmBuffer();

    bool map(QImage::Format format = QImage::Format_RGB32);
    QImage *image() const {
        return m_image;
    }
    quint32 handle() const {
        return m_handle;
    }
    const QSize &size() const {
        return m_size;
    }
    quint32 bufferId() const {
        return m_bufferId;
    }
    quint32 stride() const {
        return m_stride;
    }
    gbm_bo *gbm() const {
        return m_bo;
    }
    bool isGbm() const {
        return m_bo != nullptr;
    }
    void releaseGbm();

private:
    friend class DrmBackend;
    DrmBuffer(DrmBackend *backend, const QSize &size);
    DrmBuffer(DrmBackend *backend, gbm_surface *surface);
    DrmBackend *m_backend;
    gbm_surface *m_surface = nullptr;
    gbm_bo *m_bo = nullptr;
    QSize m_size;
    quint32 m_handle = 0;
    quint32 m_bufferId = 0;
    quint32 m_stride = 0;
    quint64 m_bufferSize = 0;
    void *m_memory = nullptr;
    QImage *m_image = nullptr;
};

class DpmsInputEventFilter : public InputEventFilter
{
public:
    DpmsInputEventFilter(DrmBackend *backend);
    ~DpmsInputEventFilter();

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(QWheelEvent *event) override;
    bool keyEvent(QKeyEvent *event) override;
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(quint32 id, quint32 time) override;

private:
    void notify();
    DrmBackend *m_backend;
    QElapsedTimer m_doubleTapTimer;
    QVector<qint32> m_touchPoints;
    bool m_secondTap = false;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput*)

#endif

