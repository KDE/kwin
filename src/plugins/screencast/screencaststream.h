/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"

#include "dmabuftexture.h"
#include "kwinglobals.h"
#include "wayland/screencast_v1_interface.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSize>
#include <QSocketNotifier>
#include <QTimer>
#include <chrono>
#include <memory>
#include <optional>

#include <pipewire/pipewire.h>
#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>

namespace KWin
{

class Cursor;
class EGLNativeFence;
class GLTexture;
class PipeWireCore;
class ScreenCastSource;

class KWIN_EXPORT ScreenCastStream : public QObject
{
    Q_OBJECT
public:
    explicit ScreenCastStream(ScreenCastSource *source, QObject *parent);
    ~ScreenCastStream();

    bool init();
    uint framerate();
    uint nodeId();
    QString error() const
    {
        return m_error;
    }

    void stop();

    /**
     * Renders @p frame into the current framebuffer into the stream
     * @p timestamp
     */
    void recordFrame(const QRegion &damagedRegion);

    void setCursorMode(KWaylandServer::ScreencastV1Interface::CursorMode mode, qreal scale, const QRect &viewport);

public Q_SLOTS:
    void recordCursor();

Q_SIGNALS:
    void streamReady(quint32 nodeId);
    void startStreaming();
    void stopStreaming();

private:
    static void onStreamParamChanged(void *data, uint32_t id, const struct spa_pod *format);
    static void onStreamStateChanged(void *data, pw_stream_state old, pw_stream_state state, const char *error_message);
    static void onStreamAddBuffer(void *data, pw_buffer *buffer);
    static void onStreamRemoveBuffer(void *data, pw_buffer *buffer);
    static void onStreamRenegotiateFormat(void *data, uint64_t);

    bool createStream();
    QVector<const spa_pod *> buildFormats(bool fixate, char buffer[2048]);
    void updateParams();
    void coreFailed(const QString &errorMessage);
    void sendCursorData(Cursor *cursor, spa_meta_cursor *spa_cursor);
    void addHeader(spa_buffer *spaBuffer);
    void addDamage(spa_buffer *spaBuffer, const QRegion &damagedRegion);
    void newStreamParams();
    void tryEnqueue(pw_buffer *buffer);
    void enqueue();
    spa_pod *buildFormat(struct spa_pod_builder *b, enum spa_video_format format, struct spa_rectangle *resolution,
                         struct spa_fraction *defaultFramerate, struct spa_fraction *minFramerate, struct spa_fraction *maxFramerate,
                         const QVector<uint64_t> &modifiers, quint32 modifiersFlags);

    std::shared_ptr<PipeWireCore> pwCore;
    std::unique_ptr<ScreenCastSource> m_source;
    struct pw_stream *pwStream = nullptr;
    struct spa_source *pwRenegotiate = nullptr;
    spa_hook streamListener;
    pw_stream_events pwStreamEvents = {};

    uint32_t pwNodeId = 0;

    QSize m_resolution;
    bool m_stopped = false;
    bool m_streaming = false;

    spa_video_info_raw videoFormat;
    QString m_error;
    QVector<uint64_t> m_modifiers;
    std::optional<DmaBufParams> m_dmabufParams; // when fixated

    struct
    {
        KWaylandServer::ScreencastV1Interface::CursorMode mode = KWaylandServer::ScreencastV1Interface::Hidden;
        const QSize bitmapSize = QSize(256, 256);
        qreal scale = 1;
        QRect viewport;
        qint64 lastKey = 0;
        QRect lastRect;
        std::unique_ptr<GLTexture> texture;
        bool visible = false;
    } m_cursor;
    QRect cursorGeometry(Cursor *cursor) const;

    QHash<struct pw_buffer *, std::shared_ptr<DmaBufTexture>> m_dmabufDataForPwBuffer;

    pw_buffer *m_pendingBuffer = nullptr;
    std::unique_ptr<QSocketNotifier> m_pendingNotifier;
    std::unique_ptr<EGLNativeFence> m_pendingFence;
    quint64 m_sequential = 0;
    bool m_hasDmaBuf = false;
    bool m_waitForNewBuffers = false;

    QDateTime m_lastSent;
    QRegion m_pendingDamages;
    QTimer m_pendingFrame;
};

} // namespace KWin
