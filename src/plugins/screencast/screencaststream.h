/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"

#include "wayland/screencast_v1.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QRegion>
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
class ScreenCastDmaBufTexture;
class EGLNativeFence;
class GLTexture;
class PipeWireCore;
class ScreenCastSource;

struct ScreenCastDmaBufTextureParams
{
    int planeCount = 0;
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;
};

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

    void setCursorMode(KWaylandServer::ScreencastV1Interface::CursorMode mode, qreal scale, const QRectF &viewport);

public Q_SLOTS:
    void invalidateCursor();
    void recordCursor();

Q_SIGNALS:
    void streamReady(quint32 nodeId);
    void startStreaming();
    void stopStreaming();

private:
    void onStreamParamChanged(uint32_t id, const struct spa_pod *format);
    void onStreamStateChanged(pw_stream_state old, pw_stream_state state, const char *error_message);
    void onStreamAddBuffer(pw_buffer *buffer);
    void onStreamRemoveBuffer(pw_buffer *buffer);
    void onStreamRenegotiateFormat(uint64_t);

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

    std::optional<ScreenCastDmaBufTextureParams> testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers);
    std::shared_ptr<ScreenCastDmaBufTexture> createDmaBufTexture(const ScreenCastDmaBufTextureParams &params);

    std::shared_ptr<PipeWireCore> m_pwCore;
    std::unique_ptr<ScreenCastSource> m_source;
    struct pw_stream *m_pwStream = nullptr;
    struct spa_source *m_pwRenegotiate = nullptr;
    spa_hook m_streamListener;
    pw_stream_events m_pwStreamEvents = {};

    uint32_t m_pwNodeId = 0;

    QSize m_resolution;
    bool m_stopped = false;
    bool m_streaming = false;

    spa_video_info_raw m_videoFormat;
    QString m_error;
    QVector<uint64_t> m_modifiers;
    std::optional<ScreenCastDmaBufTextureParams> m_dmabufParams; // when fixated

    struct
    {
        KWaylandServer::ScreencastV1Interface::CursorMode mode = KWaylandServer::ScreencastV1Interface::Hidden;
        const QSize bitmapSize = QSize(256, 256);
        qreal scale = 1;
        QRectF viewport;
        QRectF lastRect;
        std::unique_ptr<GLTexture> texture;
        bool visible = false;
        bool invalid = true;
    } m_cursor;
    QRectF cursorGeometry(Cursor *cursor) const;

    QHash<struct pw_buffer *, std::shared_ptr<ScreenCastDmaBufTexture>> m_dmabufDataForPwBuffer;

    pw_buffer *m_pendingBuffer = nullptr;
    std::unique_ptr<QSocketNotifier> m_pendingNotifier;
    std::unique_ptr<EGLNativeFence> m_pendingFence;
    quint64 m_sequential = 0;
    bool m_hasDmaBuf = false;
    bool m_waitForNewBuffers = false;
    quint32 m_drmFormat = 0;

    QDateTime m_lastSent;
    QRegion m_pendingDamages;
    QTimer m_pendingFrame;
};

} // namespace KWin
