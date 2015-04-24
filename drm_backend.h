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

#include <QImage>
#include <QSize>
#include <xf86drmMode.h>

struct gbm_bo;
struct gbm_surface;

namespace KWin
{

class Udev;
class UdevMonitor;

class DrmBuffer;
class DrmOutput;

class KWIN_EXPORT DrmBackend : public AbstractBackend
{
    Q_OBJECT
public:
    explicit DrmBackend(QObject *parent = nullptr);
    virtual ~DrmBackend();

    Screens *createScreens(QObject *parent = nullptr) override;
    QPainterBackend *createQPainterBackend() override;
    OpenGLBackend* createOpenGLBackend() override;
    void installCursorFromServer() override;
    void installCursorImage(Qt::CursorShape shape) override;

    void init();
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

Q_SIGNALS:
    void screensQueried();
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
    DrmOutput *findOutput(quint32 connector);
    QScopedPointer<Udev> m_udev;
    QScopedPointer<UdevMonitor> m_udevMonitor;
    int m_fd = -1;
    int m_drmId = 0;
    QVector<DrmOutput*> m_outputs;
    DrmBuffer *m_cursor[2];
    int m_cursorIndex = 0;
    int m_pageFlipsPending = 0;
    bool m_active = false;
};

class DrmOutput
{
public:
    virtual ~DrmOutput();
    void showCursor(DrmBuffer *buffer);
    void hideCursor();
    void moveCursor(const QPoint &globalPos);
    bool present(DrmBuffer *buffer);
    void pageFlipped();
    void init();
    void restoreSaved();
    void blank();

    QSize size() const;
    QRect geometry() const;

private:
    friend class DrmBackend;
    DrmOutput(DrmBackend *backend);
    void cleanupBlackBuffer();
    bool setMode(DrmBuffer *buffer);

    DrmBackend *m_backend;
    QPoint m_globalPos;
    quint32 m_crtcId = 0;
    quint32 m_connector = 0;
    quint32 m_lastStride = 0;
    drmModeModeInfo m_mode;
    DrmBuffer *m_currentBuffer = nullptr;
    DrmBuffer *m_blackBuffer = nullptr;
    struct CrtcCleanup {
        static void inline cleanup(_drmModeCrtc *ptr) {
            drmModeFreeCrtc(ptr);
        }
    };
    QScopedPointer<_drmModeCrtc, CrtcCleanup> m_savedCrtc;
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

}

Q_DECLARE_METATYPE(KWin::DrmOutput*)

#endif

