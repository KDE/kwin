/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "wayland/screencast_v1.h"

#include <QHash>
#include <QObject>
#include <QRegion>
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
class GLTexture;
class PipeWireCore;
class ScreenCastBuffer;
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
    explicit ScreenCastStream(ScreenCastSource *source, std::shared_ptr<PipeWireCore> pwCore, QObject *parent);
    ~ScreenCastStream();

    enum class Content {
        None,
        Video = 0x1,
        Cursor = 0x2,
    };
    Q_FLAG(Content)
    Q_DECLARE_FLAGS(Contents, Content)

    bool init();
    uint framerate();
    uint nodeId();
    QString error() const
    {
        return m_error;
    }

    void close();

    void scheduleRecord(const QRegion &damage, Contents contents = Content::Video);

    void setCursorMode(ScreencastV1Interface::CursorMode mode, qreal scale, const QRectF &viewport);

public Q_SLOTS:
    void invalidateCursor();
    bool includesCursor(Cursor *cursor) const;

Q_SIGNALS:
    void ready(quint32 nodeId);
    void closed();

private:
    void onStreamParamChanged(uint32_t id, const struct spa_pod *format);
    void onStreamStateChanged(pw_stream_state old, pw_stream_state state, const char *error_message);
    void onStreamAddBuffer(pw_buffer *buffer);
    void onStreamRemoveBuffer(pw_buffer *buffer);

    bool createStream();
    QList<const spa_pod *> buildFormats(bool fixate, char buffer[2048]);
    void updateParams();
    void resize(const QSize &resolution);
    void coreFailed(const QString &errorMessage);
    void addCursorMetadata(spa_buffer *spaBuffer, Cursor *cursor);
    QRegion addCursorEmbedded(ScreenCastBuffer *buffer, Cursor *cursor);
    void addHeader(spa_buffer *spaBuffer);
    void corruptHeader(spa_buffer *spaBuffer);
    void addDamage(spa_buffer *spaBuffer, const QRegion &damagedRegion);
    void newStreamParams();
    spa_pod *buildFormat(struct spa_pod_builder *b, enum spa_video_format format, struct spa_rectangle *resolution,
                         struct spa_fraction *defaultFramerate, struct spa_fraction *minFramerate, struct spa_fraction *maxFramerate,
                         const QList<uint64_t> &modifiers, quint32 modifiersFlags);
    void record(const QRegion &damage, Contents contents);

    std::optional<ScreenCastDmaBufTextureParams> testCreateDmaBuf(const QSize &size, quint32 format, const QList<uint64_t> &modifiers);

    std::shared_ptr<PipeWireCore> m_pwCore;
    std::unique_ptr<ScreenCastSource> m_source;
    struct pw_stream *m_pwStream = nullptr;
    spa_hook m_streamListener;
    pw_stream_events m_pwStreamEvents = {};

    uint32_t m_pwNodeId = 0;

    QSize m_resolution;
    bool m_closed = false;

    spa_video_info_raw m_videoFormat;
    QString m_error;
    QList<uint64_t> m_modifiers;
    std::optional<ScreenCastDmaBufTextureParams> m_dmabufParams; // when fixated

    struct
    {
        ScreencastV1Interface::CursorMode mode = ScreencastV1Interface::Hidden;
        const QSize bitmapSize = QSize(256, 256);
        qreal scale = 1;
        QRectF viewport;
        QRectF lastRect;
        std::unique_ptr<GLTexture> texture;
        bool visible = false;
        bool invalid = true;
        QMetaObject::Connection changedConnection = QMetaObject::Connection();
        QMetaObject::Connection positionChangedConnection = QMetaObject::Connection();
    } m_cursor;

    quint64 m_sequential = 0;
    bool m_hasDmaBuf = false;
    quint32 m_drmFormat = 0;

    std::optional<std::chrono::steady_clock::time_point> m_lastSent;
    QRegion m_pendingDamage;
    QTimer m_pendingFrame;
    Contents m_pendingContents = Content::None;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ScreenCastStream::Contents)
