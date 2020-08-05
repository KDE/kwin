/*
 * Copyright © 2018-2020 Red Hat, Inc
 * Copyright © 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jan Grulich <jgrulich@redhat.com>
 *       Aleix Pol Gonzalez <aleixpol@kde.org>
 */

#pragma once

#include "config-kwin.h"
#include "kwinglobals.h"

#include <KWaylandServer/screencast_interface.h>

#include <QHash>
#include <QObject>
#include <QSharedPointer>
#include <QSize>

#include <pipewire/pipewire.h>
#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>

namespace KWin
{

class Cursor;
class DmaBufTexture;
class GLTexture;
class PipeWireCore;

class KWIN_EXPORT PipeWireStream : public QObject
{
    Q_OBJECT
public:
    explicit PipeWireStream(bool hasAlpha, const QSize &resolution, QObject *parent);
    ~PipeWireStream();

    bool init();
    uint framerate();
    uint nodeId();
    QString error() const {
        return m_error;
    }

    void stop();

    /** Renders @p frame into the current framebuffer into the stream */
    void recordFrame(GLTexture *frame, const QRegion &damagedRegion);

    void setCursorMode(KWaylandServer::ScreencastInterface::CursorMode mode, qreal scale, const QRect &viewport);

Q_SIGNALS:
    void streamReady(quint32 nodeId);
    void startStreaming();
    void stopStreaming();

private:
    static void onStreamParamChanged(void *data, uint32_t id, const struct spa_pod *format);
    static void onStreamStateChanged(void *data, pw_stream_state old, pw_stream_state state, const char *error_message);
    static void onStreamAddBuffer(void *data, pw_buffer *buffer);
    static void onStreamRemoveBuffer(void *data, pw_buffer *buffer);

    bool createStream();
    void updateParams();
    void coreFailed(const QString &errorMessage);
    void sendCursorData(Cursor *cursor, spa_meta_cursor *spa_cursor);
    void newStreamParams();

    QSharedPointer<PipeWireCore> pwCore;
    struct pw_stream *pwStream = nullptr;
    spa_hook streamListener;
    pw_stream_events pwStreamEvents = {};

    uint32_t pwNodeId = 0;

    QSize m_resolution;
    bool m_stopped = false;

    spa_video_info_raw videoFormat;
    QString m_error;
    const bool m_hasAlpha;

    struct {
        KWaylandServer::ScreencastInterface::CursorMode mode = KWaylandServer::ScreencastInterface::Hidden;
        qreal scale = 1;
        QRect viewport;
        qint64 lastKey = 0;
        QRect lastRect;
        QScopedPointer<GLTexture> texture;
        QScopedPointer<GLTexture> lastFrameTexture;
    } m_cursor;
    bool m_repainting = false;
    QRect cursorGeometry(Cursor *cursor) const;

    QHash<struct pw_buffer *, QSharedPointer<DmaBufTexture>> m_dmabufDataForPwBuffer;
};

} // namespace KWin
