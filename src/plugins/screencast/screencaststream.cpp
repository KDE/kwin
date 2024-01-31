/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencaststream.h"
#include "composite.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "cursor.h"
#include "dmabuftexture.h"
#include "eglnativefence.h"
#include "kwineffects.h"
#include "kwinglplatform.h"
#include "kwingltexture.h"
#include "kwinglutils.h"
#include "kwinscreencast_logging.h"
#include "main.h"
#include "openglbackend.h"
#include "pipewirecore.h"
#include "scene/workspacescene.h"
#include "screencastsource.h"
#include "utils/common.h"

#include <KLocalizedString>

#include <QLoggingCategory>
#include <QPainter>

#include <spa/buffer/meta.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <libdrm/drm_fourcc.h>

namespace KWin
{

uint32_t spaVideoFormatToDrmFormat(spa_video_format spa_format)
{
    switch (spa_format) {
    case SPA_VIDEO_FORMAT_RGBA:
        return DRM_FORMAT_ABGR8888;
    case SPA_VIDEO_FORMAT_RGBx:
        return DRM_FORMAT_XBGR8888;
    case SPA_VIDEO_FORMAT_BGRA:
        return DRM_FORMAT_ARGB8888;
    case SPA_VIDEO_FORMAT_BGRx:
        return DRM_FORMAT_XRGB8888;
    case SPA_VIDEO_FORMAT_BGR:
        return DRM_FORMAT_BGR888;
    case SPA_VIDEO_FORMAT_RGB:
        return DRM_FORMAT_RGB888;
    default:
        qCDebug(KWIN_SCREENCAST) << "unknown format" << spa_format;
        return DRM_FORMAT_INVALID;
    }
}

static QImage::Format SpaToQImageFormat(quint32 format)
{
    switch (format) {
    case SPA_VIDEO_FORMAT_BGRx:
    case SPA_VIDEO_FORMAT_BGRA:
        return QImage::Format_RGBA8888_Premultiplied; // TODO: Add BGR to QImage
    case SPA_VIDEO_FORMAT_BGR:
        return QImage::Format_BGR888;
    case SPA_VIDEO_FORMAT_RGBx:
        return QImage::Format_RGBX8888;
    case SPA_VIDEO_FORMAT_RGB:
        return QImage::Format_RGB888;
    case SPA_VIDEO_FORMAT_RGBA:
        return QImage::Format_RGBA8888_Premultiplied;
    default:
        qCWarning(KWIN_SCREENCAST) << "unknown spa format" << format;
        return QImage::Format_RGB32;
    }
}

void ScreenCastStream::onStreamStateChanged(void *data, pw_stream_state old, pw_stream_state state, const char *error_message)
{
    ScreenCastStream *pw = static_cast<ScreenCastStream *>(data);
    qCDebug(KWIN_SCREENCAST) << "state changed" << pw_stream_state_as_string(old) << " -> " << pw_stream_state_as_string(state) << error_message;

    pw->m_streaming = false;
    pw->m_pendingBuffer = nullptr;
    pw->m_pendingNotifier.reset();
    pw->m_pendingFence.reset();

    switch (state) {
    case PW_STREAM_STATE_ERROR:
        qCWarning(KWIN_SCREENCAST) << "Stream error: " << error_message;
        break;
    case PW_STREAM_STATE_PAUSED:
        if (pw->nodeId() == 0 && pw->pwStream) {
            pw->pwNodeId = pw_stream_get_node_id(pw->pwStream);
            Q_EMIT pw->streamReady(pw->nodeId());
        }
        break;
    case PW_STREAM_STATE_STREAMING:
        pw->m_streaming = true;
        Q_EMIT pw->startStreaming();
        break;
    case PW_STREAM_STATE_CONNECTING:
        break;
    case PW_STREAM_STATE_UNCONNECTED:
        if (!pw->m_stopped) {
            Q_EMIT pw->stopStreaming();
        }
        break;
    }
}

#define CURSOR_BPP 4
#define CURSOR_META_SIZE(w, h) (sizeof(struct spa_meta_cursor) + sizeof(struct spa_meta_bitmap) + w * h * CURSOR_BPP)
static const int videoDamageRegionCount = 16;

void ScreenCastStream::newStreamParams()
{
    qCDebug(KWIN_SCREENCAST) << "announcing stream params. with dmabuf:" << m_dmabufParams.has_value();
    uint8_t paramsBuffer[1024];
    spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT(paramsBuffer, sizeof(paramsBuffer));
    const int buffertypes = m_dmabufParams ? (1 << SPA_DATA_DmaBuf) | (1 << SPA_DATA_MemFd) : (1 << SPA_DATA_MemFd);
    const int bpp = videoFormat.format == SPA_VIDEO_FORMAT_RGB || videoFormat.format == SPA_VIDEO_FORMAT_BGR ? 3 : 4;
    const int stride = SPA_ROUND_UP_N(m_resolution.width() * bpp, 4);

    struct spa_pod_frame f;
    spa_pod_builder_push_object(&pod_builder, &f, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers);
    spa_pod_builder_add(&pod_builder,
                        SPA_PARAM_BUFFERS_size, SPA_POD_Int(stride * m_resolution.height()),
                        SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(16, 2, 16),
                        SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
                        SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(buffertypes), 0);
    if (!m_dmabufParams) {
        spa_pod_builder_add(&pod_builder,
                            SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
                            SPA_PARAM_BUFFERS_align, SPA_POD_Int(16), 0);
    } else {
        spa_pod_builder_add(&pod_builder,
                            SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(m_dmabufParams->planeCount), 0);
    }
    spa_pod *bufferPod = (spa_pod *)spa_pod_builder_pop(&pod_builder, &f);

    QVarLengthArray<const spa_pod *> params = {
        bufferPod,
        (spa_pod *)spa_pod_builder_add_object(&pod_builder,
                                              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                              SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Cursor),
                                              SPA_PARAM_META_size, SPA_POD_Int(CURSOR_META_SIZE(m_cursor.bitmapSize.width(), m_cursor.bitmapSize.height()))),
        (spa_pod *)spa_pod_builder_add_object(&pod_builder,
                                              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                              SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoDamage),
                                              SPA_PARAM_META_size, SPA_POD_CHOICE_RANGE_Int(sizeof(struct spa_meta_region) * videoDamageRegionCount, sizeof(struct spa_meta_region) * 1, sizeof(struct spa_meta_region) * videoDamageRegionCount)),
        (spa_pod *)spa_pod_builder_add_object(&pod_builder,
                                              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                              SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
                                              SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header))),
    };

    pw_stream_update_params(pwStream, params.data(), params.count());
}

void ScreenCastStream::onStreamParamChanged(void *data, uint32_t id, const struct spa_pod *format)
{
    if (!format || id != SPA_PARAM_Format) {
        return;
    }

    ScreenCastStream *pw = static_cast<ScreenCastStream *>(data);
    spa_format_video_raw_parse(format, &pw->videoFormat);
    auto modifierProperty = spa_pod_find_prop(format, nullptr, SPA_FORMAT_VIDEO_modifier);
    QVector<uint64_t> receivedModifiers;
    if (modifierProperty) {
        const struct spa_pod *modifierPod = &modifierProperty->value;

        uint32_t modifiersCount = SPA_POD_CHOICE_N_VALUES(modifierPod);
        uint64_t *modifiers = (uint64_t *)SPA_POD_CHOICE_VALUES(modifierPod);
        receivedModifiers = QVector<uint64_t>(modifiers, modifiers + modifiersCount);
    }
    if (modifierProperty && (!pw->m_dmabufParams || !receivedModifiers.contains(pw->m_dmabufParams->modifier))) {
        if (modifierProperty->flags & SPA_POD_PROP_FLAG_DONT_FIXATE) {
            pw->m_dmabufParams = kwinApp()->outputBackend()->testCreateDmaBuf(pw->m_resolution, spaVideoFormatToDrmFormat(pw->videoFormat.format), receivedModifiers);
        } else {
            pw->m_dmabufParams = kwinApp()->outputBackend()->testCreateDmaBuf(pw->m_resolution, spaVideoFormatToDrmFormat(pw->videoFormat.format), {DRM_FORMAT_MOD_INVALID});
        }

        qCDebug(KWIN_SCREENCAST) << "Stream dmabuf modifiers received, offering our best suited modifier" << pw->m_dmabufParams.has_value();
        char buffer[2048];
        auto params = pw->buildFormats(pw->m_dmabufParams.has_value(), buffer);
        pw_stream_update_params(pw->pwStream, params.data(), params.count());
        return;
    }

    qCDebug(KWIN_SCREENCAST) << "Stream format found, defining buffers";
    pw->newStreamParams();
}

void ScreenCastStream::onStreamAddBuffer(void *data, pw_buffer *buffer)
{
    ScreenCastStream *stream = static_cast<ScreenCastStream *>(data);
    struct spa_data *spa_data = buffer->buffer->datas;

    spa_data->mapoffset = 0;
    spa_data->flags = SPA_DATA_FLAG_READWRITE;

    std::shared_ptr<DmaBufTexture> dmabuff;

    if (spa_data[0].type != SPA_ID_INVALID && spa_data[0].type & (1 << SPA_DATA_DmaBuf)) {
        Q_ASSERT(stream->m_dmabufParams);
        dmabuff = kwinApp()->outputBackend()->createDmaBufTexture(*stream->m_dmabufParams);
    }

    if (dmabuff) {
        spa_data->maxsize = dmabuff->attributes().pitch[0] * stream->m_resolution.height();

        const DmaBufAttributes &dmabufAttribs = dmabuff->attributes();
        Q_ASSERT(buffer->buffer->n_datas >= uint(dmabufAttribs.planeCount));
        for (int i = 0; i < dmabufAttribs.planeCount; ++i) {
            buffer->buffer->datas[i].type = SPA_DATA_DmaBuf;
            buffer->buffer->datas[i].fd = dmabufAttribs.fd[i].get();
            buffer->buffer->datas[i].data = nullptr;
        }
        stream->m_dmabufDataForPwBuffer.insert(buffer, dmabuff);
#ifdef F_SEAL_SEAL // Disable memfd on systems that don't have it, like BSD < 12
    } else {
        if (!(spa_data[0].type & (1 << SPA_DATA_MemFd))) {
            qCCritical(KWIN_SCREENCAST) << "memfd: Client doesn't support memfd buffer data type";
            return;
        }

        const int bytesPerPixel = stream->m_source->hasAlphaChannel() ? 4 : 3;
        const int stride = SPA_ROUND_UP_N(stream->m_resolution.width() * bytesPerPixel, 4);
        spa_data->maxsize = stride * stream->m_resolution.height();
        spa_data->type = SPA_DATA_MemFd;
        spa_data->fd = memfd_create("kwin-screencast-memfd", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (spa_data->fd == -1) {
            qCCritical(KWIN_SCREENCAST) << "memfd: Can't create memfd";
            return;
        }
        spa_data->mapoffset = 0;

        if (ftruncate(spa_data->fd, spa_data->maxsize) < 0) {
            qCCritical(KWIN_SCREENCAST) << "memfd: Can't truncate to" << spa_data->maxsize;
            return;
        }

        unsigned int seals = F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
        if (fcntl(spa_data->fd, F_ADD_SEALS, seals) == -1) {
            qCWarning(KWIN_SCREENCAST) << "memfd: Failed to add seals";
        }

        spa_data->data = mmap(nullptr,
                              spa_data->maxsize,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED,
                              spa_data->fd,
                              spa_data->mapoffset);
        if (spa_data->data == MAP_FAILED) {
            qCCritical(KWIN_SCREENCAST) << "memfd: Failed to mmap memory";
        } else {
            qCDebug(KWIN_SCREENCAST) << "memfd: created successfully" << spa_data->data << spa_data->maxsize;
        }
#endif
    }

    stream->m_waitForNewBuffers = false;
}

void ScreenCastStream::onStreamRemoveBuffer(void *data, pw_buffer *buffer)
{
    ScreenCastStream *stream = static_cast<ScreenCastStream *>(data);
    stream->m_dmabufDataForPwBuffer.remove(buffer);

    struct spa_buffer *spa_buffer = buffer->buffer;
    struct spa_data *spa_data = spa_buffer->datas;
    if (spa_data && spa_data->type == SPA_DATA_MemFd) {
        munmap(spa_data->data, spa_data->maxsize);
        close(spa_data->fd);
    } else if (spa_data && spa_data->type == SPA_DATA_DmaBuf) {
        for (int i = 0, c = buffer->buffer->n_datas; i < c; ++i) {
            close(buffer->buffer->datas[i].fd);
        }
    }
}

void ScreenCastStream::onStreamRenegotiateFormat(void *data, uint64_t)
{
    ScreenCastStream *stream = static_cast<ScreenCastStream *>(data);

    stream->m_streaming = false; // pause streaming as we wait for the renegotiation
    char buffer[2048];
    auto params = stream->buildFormats(stream->m_dmabufParams.has_value(), buffer);
    pw_stream_update_params(stream->pwStream, params.data(), params.count());
}

ScreenCastStream::ScreenCastStream(ScreenCastSource *source, QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_resolution(source->textureSize())
{
    connect(source, &ScreenCastSource::closed, this, [this] {
        m_streaming = false;
        Q_EMIT stopStreaming();
    });

    pwStreamEvents.version = PW_VERSION_STREAM_EVENTS;
    pwStreamEvents.add_buffer = &ScreenCastStream::onStreamAddBuffer;
    pwStreamEvents.remove_buffer = &ScreenCastStream::onStreamRemoveBuffer;
    pwStreamEvents.state_changed = &ScreenCastStream::onStreamStateChanged;
    pwStreamEvents.param_changed = &ScreenCastStream::onStreamParamChanged;

    m_pendingFrame.setSingleShot(true);
    connect(&m_pendingFrame, &QTimer::timeout, this, [this] {
        recordFrame(m_pendingDamages);
    });
}

ScreenCastStream::~ScreenCastStream()
{
    m_stopped = true;
    if (pwStream) {
        pw_stream_destroy(pwStream);
    }
}

bool ScreenCastStream::init()
{
    pwCore = PipeWireCore::self();
    if (!pwCore->m_error.isEmpty()) {
        m_error = pwCore->m_error;
        return false;
    }

    connect(pwCore.get(), &PipeWireCore::pipewireFailed, this, &ScreenCastStream::coreFailed);

    if (!createStream()) {
        qCWarning(KWIN_SCREENCAST) << "Failed to create PipeWire stream";
        m_error = i18n("Failed to create PipeWire stream");
        return false;
    }

    pwRenegotiate = pw_loop_add_event(pwCore.get()->pwMainLoop, onStreamRenegotiateFormat, this);

    return true;
}

uint ScreenCastStream::framerate()
{
    if (pwStream) {
        return videoFormat.max_framerate.num / videoFormat.max_framerate.denom;
    }

    return 0;
}

uint ScreenCastStream::nodeId()
{
    return pwNodeId;
}

bool ScreenCastStream::createStream()
{
    const QByteArray objname = "kwin-screencast-" + objectName().toUtf8();
    pwStream = pw_stream_new(pwCore->pwCore, objname, nullptr);

    // it could make sense to offer the same format as the source
    const auto format = m_source->hasAlphaChannel() ? SPA_VIDEO_FORMAT_BGRA : SPA_VIDEO_FORMAT_BGR;
    const int drmFormat = spaVideoFormatToDrmFormat(format);
    m_hasDmaBuf = kwinApp()->outputBackend()->testCreateDmaBuf(m_resolution, drmFormat, {DRM_FORMAT_MOD_INVALID}).has_value();
    m_modifiers = Compositor::self()->backend()->supportedFormats().value(drmFormat);

    char buffer[2048];
    QVector<const spa_pod *> params = buildFormats(false, buffer);

    pw_stream_add_listener(pwStream, &streamListener, &pwStreamEvents, this);
    auto flags = pw_stream_flags(PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_ALLOC_BUFFERS);

    if (pw_stream_connect(pwStream, PW_DIRECTION_OUTPUT, SPA_ID_INVALID, flags, params.data(), params.count()) != 0) {
        qCWarning(KWIN_SCREENCAST) << "Could not connect to stream";
        pw_stream_destroy(pwStream);
        pwStream = nullptr;
        return false;
    }

    if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded) {
        connect(Cursors::self(), &Cursors::positionChanged, this, [this] {
            recordFrame({});
        });
    } else if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Metadata) {
        connect(Cursors::self(), &Cursors::positionChanged, this, &ScreenCastStream::recordCursor);
    }

    return true;
}
void ScreenCastStream::coreFailed(const QString &errorMessage)
{
    m_error = errorMessage;
    Q_EMIT stopStreaming();
}

void ScreenCastStream::stop()
{
    m_stopped = true;
    delete this;
}

void ScreenCastStream::recordFrame(const QRegion &_damagedRegion)
{
    QRegion damagedRegion = _damagedRegion;
    Q_ASSERT(!m_stopped);

    if (!m_streaming) {
        m_pendingDamages |= damagedRegion;
        return;
    }

    if (videoFormat.max_framerate.num != 0 && !m_lastSent.isNull()) {
        auto frameInterval = (1000. * videoFormat.max_framerate.denom / videoFormat.max_framerate.num);
        auto lastSentAgo = m_lastSent.msecsTo(QDateTime::currentDateTimeUtc());
        if (lastSentAgo < frameInterval) {
            m_pendingDamages |= damagedRegion;
            if (!m_pendingFrame.isActive()) {
                m_pendingFrame.start(frameInterval - lastSentAgo);
            }
            return;
        }
    }

    m_pendingDamages = {};
    if (m_pendingBuffer) {
        return;
    }

    if (m_waitForNewBuffers) {
        qCWarning(KWIN_SCREENCAST) << "Waiting for new buffers to be created";
        return;
    }

    const auto size = m_source->textureSize();
    if (size != m_resolution) {
        m_resolution = size;
        m_waitForNewBuffers = true;
        m_dmabufParams = std::nullopt;
        pw_loop_signal_event(pwCore.get()->pwMainLoop, pwRenegotiate);
        return;
    }

    const char *error = "";
    auto state = pw_stream_get_state(pwStream, &error);
    if (state != PW_STREAM_STATE_STREAMING) {
        if (error) {
            qCWarning(KWIN_SCREENCAST) << "Failed to record frame: stream is not active" << error;
        }
        return;
    }

    struct pw_buffer *buffer = pw_stream_dequeue_buffer(pwStream);

    if (!buffer) {
        return;
    }

    struct spa_buffer *spa_buffer = buffer->buffer;
    struct spa_data *spa_data = spa_buffer->datas;

    uint8_t *data = (uint8_t *)spa_data->data;
    if (!data && spa_buffer->datas->type != SPA_DATA_DmaBuf) {
        qCWarning(KWIN_SCREENCAST) << "Failed to record frame: invalid buffer data";
        pw_stream_queue_buffer(pwStream, buffer);
        return;
    }

    spa_data->chunk->offset = 0;
    spa_data->chunk->flags = SPA_CHUNK_FLAG_NONE;
    static_cast<OpenGLBackend *>(Compositor::self()->backend())->makeCurrent();
    if (data || spa_data[0].type == SPA_DATA_MemFd) {
        const bool hasAlpha = m_source->hasAlphaChannel();
        const int bpp = data && !hasAlpha ? 3 : 4;
        const uint stride = SPA_ROUND_UP_N(size.width() * bpp, 4);

        if ((stride * size.height()) > spa_data->maxsize) {
            qCDebug(KWIN_SCREENCAST) << "Failed to record frame: frame is too big";
            pw_stream_queue_buffer(pwStream, buffer);
            return;
        }

        spa_data->chunk->stride = stride;
        spa_data->chunk->size = stride * size.height();

        m_source->render(spa_data, videoFormat.format);

        auto cursor = Cursors::self()->currentCursor();
        if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded && m_cursor.viewport.contains(cursor->pos())) {
            QImage dest(data, size.width(), size.height(), stride, SpaToQImageFormat(videoFormat.format));
            QPainter painter(&dest);
            const auto position = (cursor->pos() - m_cursor.viewport.topLeft() - cursor->hotspot()) * m_cursor.scale;
            painter.drawImage(QRect{position, cursor->image().size()}, cursor->image());
        }
    } else {
        auto &buf = m_dmabufDataForPwBuffer[buffer];
        Q_ASSERT(buf);

        const DmaBufAttributes &dmabufAttribs = buf->attributes();
        Q_ASSERT(buffer->buffer->n_datas >= uint(dmabufAttribs.planeCount));
        for (int i = 0; i < dmabufAttribs.planeCount; ++i) {
            buffer->buffer->datas[i].chunk->stride = dmabufAttribs.pitch[i];
            buffer->buffer->datas[i].chunk->offset = dmabufAttribs.offset[i];
        }
        spa_data->chunk->size = spa_data->maxsize;

        m_source->render(buf->framebuffer());

        auto cursor = Cursors::self()->currentCursor();
        if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded && m_cursor.viewport.contains(cursor->pos())) {
            if (!cursor->image().isNull()) {
                GLFramebuffer::pushFramebuffer(buf->framebuffer());

                QRect r(QPoint(), size);
                auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

                QMatrix4x4 mvp;
                mvp.ortho(r);
                shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

                if (!m_cursor.texture || m_cursor.lastKey != cursor->image().cacheKey()) {
                    m_cursor.texture.reset(new GLTexture(cursor->image()));
                }

                m_cursor.texture->setYInverted(false);
                m_cursor.texture->bind();
                const auto cursorRect = cursorGeometry(cursor);
                mvp.translate(cursorRect.left(), r.height() - cursorRect.top() - cursor->image().height());
                shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                m_cursor.texture->render(cursorRect, m_cursor.scale);
                glDisable(GL_BLEND);
                m_cursor.texture->unbind();

                ShaderManager::instance()->popShader();
                GLFramebuffer::popFramebuffer();

                damagedRegion += QRegion{m_cursor.lastRect} | cursorRect;
                m_cursor.lastRect = cursorRect;
            } else {
                damagedRegion |= m_cursor.lastRect;
                m_cursor.lastRect = {};
            }
        }
    }

    if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Metadata) {
        sendCursorData(Cursors::self()->currentCursor(),
                       (spa_meta_cursor *)spa_buffer_find_meta_data(spa_buffer, SPA_META_Cursor, sizeof(spa_meta_cursor)));
    }

    addDamage(spa_buffer, damagedRegion);
    addHeader(spa_buffer);
    tryEnqueue(buffer);
}

void ScreenCastStream::addHeader(spa_buffer *spaBuffer)
{
    spa_meta_header *spaHeader = (spa_meta_header *)spa_buffer_find_meta_data(spaBuffer, SPA_META_Header, sizeof(spaHeader));
    if (spaHeader) {
        spaHeader->flags = 0;
        spaHeader->dts_offset = 0;
        spaHeader->seq = m_sequential++;
        spaHeader->pts = m_source->clock().count();
    }
}

void ScreenCastStream::addDamage(spa_buffer *spaBuffer, const QRegion &damagedRegion)
{
    if (spa_meta *vdMeta = spa_buffer_find_meta(spaBuffer, SPA_META_VideoDamage)) {
        struct spa_meta_region *r = (spa_meta_region *)spa_meta_first(vdMeta);

        // If there's too many rectangles, we just send the bounding rect
        if (damagedRegion.rectCount() > videoDamageRegionCount - 1) {
            if (spa_meta_check(r, vdMeta)) {
                auto rect = damagedRegion.boundingRect();
                r->region = SPA_REGION(rect.x(), rect.y(), quint32(rect.width()), quint32(rect.height()));
                r++;
            }
        } else {
            for (const QRect &rect : damagedRegion) {
                if (spa_meta_check(r, vdMeta)) {
                    r->region = SPA_REGION(rect.x(), rect.y(), quint32(rect.width()), quint32(rect.height()));
                    r++;
                }
            }
        }

        if (spa_meta_check(r, vdMeta)) {
            r->region = SPA_REGION(0, 0, 0, 0);
        }
    }
}

void ScreenCastStream::recordCursor()
{
    Q_ASSERT(!m_stopped);
    if (!m_streaming) {
        return;
    }

    if (m_pendingBuffer) {
        return;
    }

    const char *error = "";
    auto state = pw_stream_get_state(pwStream, &error);
    if (state != PW_STREAM_STATE_STREAMING) {
        if (error) {
            qCWarning(KWIN_SCREENCAST) << "Failed to record cursor position: stream is not active" << error;
        }
        return;
    }

    if (!m_cursor.viewport.contains(Cursors::self()->currentCursor()->pos()) && !m_cursor.visible) {
        return;
    }

    m_pendingBuffer = pw_stream_dequeue_buffer(pwStream);
    if (!m_pendingBuffer) {
        return;
    }

    struct spa_buffer *spa_buffer = m_pendingBuffer->buffer;

    // in pipewire terms, corrupted means "do not look at the frame contents" and here they're empty.
    spa_buffer->datas[0].chunk->flags = SPA_CHUNK_FLAG_CORRUPTED;
    spa_buffer->datas[0].chunk->size = 0;

    sendCursorData(Cursors::self()->currentCursor(),
                   (spa_meta_cursor *)spa_buffer_find_meta_data(spa_buffer, SPA_META_Cursor, sizeof(spa_meta_cursor)));
    addHeader(spa_buffer);
    addDamage(spa_buffer, {});
    enqueue();
}

void ScreenCastStream::tryEnqueue(pw_buffer *buffer)
{
    m_pendingBuffer = buffer;

    // The GPU doesn't necessarily process draw commands as soon as they are issued. Thus,
    // we need to insert a fence into the command stream and enqueue the pipewire buffer
    // only after the fence is signaled; otherwise stream consumers will most likely see
    // a corrupted buffer.
    if (Compositor::self()->scene()->supportsNativeFence()) {
        Q_ASSERT_X(eglGetCurrentContext(), "tryEnqueue", "no current context");
        m_pendingFence = std::make_unique<EGLNativeFence>(kwinApp()->outputBackend()->sceneEglDisplay());
        if (!m_pendingFence->isValid()) {
            qCWarning(KWIN_SCREENCAST) << "Failed to create a native EGL fence";
            glFinish();
            enqueue();
        } else {
            m_pendingNotifier = std::make_unique<QSocketNotifier>(m_pendingFence->fileDescriptor(), QSocketNotifier::Read);
            connect(m_pendingNotifier.get(), &QSocketNotifier::activated, this, &ScreenCastStream::enqueue);
        }
    } else {
        // The compositing backend doesn't support native fences. We don't have any other choice
        // but stall the graphics pipeline. Otherwise stream consumers may see an incomplete buffer.
        glFinish();
        enqueue();
    }
}

void ScreenCastStream::enqueue()
{
    Q_ASSERT_X(m_pendingBuffer, "enqueue", "pending buffer must be valid");

    m_pendingFence.reset();
    m_pendingNotifier.reset();

    if (!m_streaming) {
        return;
    }
    pw_stream_queue_buffer(pwStream, m_pendingBuffer);

    if (m_pendingBuffer->buffer->datas[0].chunk->flags != SPA_CHUNK_FLAG_CORRUPTED) {
        m_lastSent = QDateTime::currentDateTimeUtc();
    }

    m_pendingBuffer = nullptr;
}

QVector<const spa_pod *> ScreenCastStream::buildFormats(bool fixate, char buffer[2048])
{
    const auto format = m_source->hasAlphaChannel() ? SPA_VIDEO_FORMAT_BGRA : SPA_VIDEO_FORMAT_BGR;
    spa_pod_builder podBuilder = SPA_POD_BUILDER_INIT(buffer, 2048);
    spa_fraction defFramerate = SPA_FRACTION(0, 1);
    spa_fraction minFramerate = SPA_FRACTION(1, 1);
    spa_fraction maxFramerate = SPA_FRACTION(m_source->refreshRate() / 1000, 1);

    spa_rectangle resolution = SPA_RECTANGLE(uint32_t(m_resolution.width()), uint32_t(m_resolution.height()));

    QVector<const spa_pod *> params;
    params.reserve(fixate + m_hasDmaBuf + 1);
    if (fixate) {
        params.append(buildFormat(&podBuilder, SPA_VIDEO_FORMAT_BGRA, &resolution, &defFramerate, &minFramerate, &maxFramerate, {m_dmabufParams->modifier}, SPA_POD_PROP_FLAG_MANDATORY));
    }
    if (m_hasDmaBuf) {
        params.append(buildFormat(&podBuilder, SPA_VIDEO_FORMAT_BGRA, &resolution, &defFramerate, &minFramerate, &maxFramerate, m_modifiers, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE));
    }
    params.append(buildFormat(&podBuilder, format, &resolution, &defFramerate, &minFramerate, &maxFramerate, {}, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE));
    return params;
}

spa_pod *ScreenCastStream::buildFormat(struct spa_pod_builder *b, enum spa_video_format format, struct spa_rectangle *resolution,
                                       struct spa_fraction *defaultFramerate, struct spa_fraction *minFramerate, struct spa_fraction *maxFramerate,
                                       const QVector<uint64_t> &modifiers, quint32 modifiersFlags)
{
    struct spa_pod_frame f[2];
    spa_pod_builder_push_object(b, &f[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
    spa_pod_builder_add(b, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(resolution), 0);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(defaultFramerate), 0);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_maxFramerate,
                        SPA_POD_CHOICE_RANGE_Fraction(
                            SPA_POD_Fraction(maxFramerate),
                            SPA_POD_Fraction(minFramerate),
                            SPA_POD_Fraction(maxFramerate)),
                        0);

    if (format == SPA_VIDEO_FORMAT_BGRA) {
        /* announce equivalent format without alpha */
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(3, format, format, SPA_VIDEO_FORMAT_BGRx), 0);
    } else {
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);
    }

    if (!modifiers.isEmpty()) {
        spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_modifier, modifiersFlags);
        spa_pod_builder_push_choice(b, &f[1], SPA_CHOICE_Enum, 0);

        int c = 0;
        for (auto modifier : modifiers) {
            spa_pod_builder_long(b, modifier);
            if (c++ == 0) {
                spa_pod_builder_long(b, modifier);
            }
        }
        spa_pod_builder_pop(b, &f[1]);
    }
    return (spa_pod *)spa_pod_builder_pop(b, &f[0]);
}

QRect ScreenCastStream::cursorGeometry(Cursor *cursor) const
{
    if (!m_cursor.texture) {
        return {};
    }

    const auto position = (cursor->pos() - m_cursor.viewport.topLeft() - cursor->hotspot()) * m_cursor.scale;
    return QRect{position, m_cursor.texture->size()};
}

void ScreenCastStream::sendCursorData(Cursor *cursor, spa_meta_cursor *spa_meta_cursor)
{
    if (!cursor || !spa_meta_cursor) {
        return;
    }

    if (!m_cursor.viewport.contains(cursor->pos())) {
        spa_meta_cursor->id = 0;
        spa_meta_cursor->position.x = -1;
        spa_meta_cursor->position.y = -1;
        spa_meta_cursor->hotspot.x = -1;
        spa_meta_cursor->hotspot.y = -1;
        spa_meta_cursor->bitmap_offset = 0;
        m_cursor.visible = false;
        return;
    }
    m_cursor.visible = true;
    const auto position = (cursor->pos() - m_cursor.viewport.topLeft()) * m_cursor.scale;

    spa_meta_cursor->id = 1;
    spa_meta_cursor->position.x = position.x();
    spa_meta_cursor->position.y = position.y();
    spa_meta_cursor->hotspot.x = cursor->hotspot().x() * m_cursor.scale;
    spa_meta_cursor->hotspot.y = cursor->hotspot().y() * m_cursor.scale;
    spa_meta_cursor->bitmap_offset = 0;

    const QImage image = cursor->image();
    if (image.cacheKey() == m_cursor.lastKey) {
        return;
    }

    m_cursor.lastKey = image.cacheKey();
    spa_meta_cursor->bitmap_offset = sizeof(struct spa_meta_cursor);

    const QSize targetSize = cursor->rect().size() * m_cursor.scale;

    struct spa_meta_bitmap *spa_meta_bitmap = SPA_MEMBER(spa_meta_cursor,
                                                         spa_meta_cursor->bitmap_offset,
                                                         struct spa_meta_bitmap);
    spa_meta_bitmap->format = SPA_VIDEO_FORMAT_RGBA;
    spa_meta_bitmap->offset = sizeof(struct spa_meta_bitmap);
    spa_meta_bitmap->size.width = std::min(m_cursor.bitmapSize.width(), targetSize.width());
    spa_meta_bitmap->size.height = std::min(m_cursor.bitmapSize.height(), targetSize.height());
    spa_meta_bitmap->stride = spa_meta_bitmap->size.width * 4;

    uint8_t *bitmap_data = SPA_MEMBER(spa_meta_bitmap, spa_meta_bitmap->offset, uint8_t);
    QImage dest(bitmap_data,
                spa_meta_bitmap->size.width,
                spa_meta_bitmap->size.height,
                spa_meta_bitmap->stride,
                QImage::Format_RGBA8888_Premultiplied);
    dest.fill(Qt::transparent);

    if (!image.isNull()) {
        QPainter painter(&dest);
        painter.drawImage(QRect({0, 0}, targetSize), image);
    }
}

void ScreenCastStream::setCursorMode(KWaylandServer::ScreencastV1Interface::CursorMode mode, qreal scale, const QRect &viewport)
{
    m_cursor.mode = mode;
    m_cursor.scale = scale;
    m_cursor.viewport = viewport;
}

} // namespace KWin
