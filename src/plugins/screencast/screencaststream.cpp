/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencaststream.h"
#include "cursor.h"
#include "composite.h"
#include "dmabuftexture.h"
#include "eglnativefence.h"
#include "kwinglplatform.h"
#include "kwingltexture.h"
#include "kwinglutils.h"
#include "kwinscreencast_logging.h"
#include "main.h"
#include "pipewirecore.h"
#include "platform.h"
#include "scene.h"
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

void ScreenCastStream::onStreamStateChanged(void *data, pw_stream_state old, pw_stream_state state, const char *error_message)
{
    ScreenCastStream *pw = static_cast<ScreenCastStream*>(data);
    qCDebug(KWIN_SCREENCAST) << "state changed"<< pw_stream_state_as_string(old) << " -> " << pw_stream_state_as_string(state) << error_message;

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

#define CURSOR_BPP	4
#define CURSOR_META_SIZE(w,h)	(sizeof(struct spa_meta_cursor) + \
				 sizeof(struct spa_meta_bitmap) + w * h * CURSOR_BPP)
static const int videoDamageRegionCount = 16;

void ScreenCastStream::newStreamParams()
{
    const int bpp = videoFormat.format == SPA_VIDEO_FORMAT_RGB || videoFormat.format == SPA_VIDEO_FORMAT_BGR ? 3 : 4;
    auto stride = SPA_ROUND_UP_N (m_resolution.width() * bpp, 4);

    uint8_t paramsBuffer[1024];
    spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT (paramsBuffer, sizeof (paramsBuffer));
    int buffertypes;

    if (m_hasModifier) {
        buffertypes = (1<<SPA_DATA_DmaBuf);
    } else {
        buffertypes = (1<<SPA_DATA_MemFd);
    }

    spa_rectangle resolution = SPA_RECTANGLE(uint32_t(m_resolution.width()), uint32_t(m_resolution.height()));
    const spa_pod *params[] = {
        (spa_pod*) spa_pod_builder_add_object(&pod_builder,
                                              SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                                              SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&resolution),
                                              SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(16, 2, 16),
                                              SPA_PARAM_BUFFERS_blocks, SPA_POD_Int (1),
                                              // TODO[no_use_linear]: stride, size and align should be dropped for dmabufs,
                                              // or queried via test allocation
                                              SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
                                              SPA_PARAM_BUFFERS_size, SPA_POD_Int(stride * m_resolution.height()),
                                              SPA_PARAM_BUFFERS_align, SPA_POD_Int(16),
                                              SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(buffertypes)),
        (spa_pod*) spa_pod_builder_add_object (&pod_builder,
                                               SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                               SPA_PARAM_META_type, SPA_POD_Id (SPA_META_Cursor),
                                               SPA_PARAM_META_size, SPA_POD_Int (CURSOR_META_SIZE (m_cursor.bitmapSize.width(), m_cursor.bitmapSize.height()))),
        (spa_pod*) spa_pod_builder_add_object(&pod_builder,
                                                SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                                SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoDamage),
                                                SPA_PARAM_META_size, SPA_POD_CHOICE_RANGE_Int(
                                                            sizeof(struct spa_meta_region) * videoDamageRegionCount,
                                                            sizeof(struct spa_meta_region) * 1,
                                                            sizeof(struct spa_meta_region) * videoDamageRegionCount)),
    };

    pw_stream_update_params(pwStream, params, 3);
}

void ScreenCastStream::onStreamParamChanged(void *data, uint32_t id, const struct spa_pod *format)
{
    if (!format || id != SPA_PARAM_Format) {
        return;
    }

    ScreenCastStream *pw = static_cast<ScreenCastStream *>(data);
    spa_format_video_raw_parse (format, &pw->videoFormat);
    // TODO[explicit_modifiers]: check if modifier list or single modifier,
    // make test allocation, fixate format, ...
    // depends on how flexible kwin will become in that regard.
    pw->m_hasModifier = spa_pod_find_prop(format, nullptr, SPA_FORMAT_VIDEO_modifier) != nullptr;
    qCDebug(KWIN_SCREENCAST) << "Stream format changed" << pw << pw->videoFormat.format;
    pw->newStreamParams();
}

void ScreenCastStream::onStreamAddBuffer(void *data, pw_buffer *buffer)
{
    QSharedPointer<DmaBufTexture> dmabuf;
    ScreenCastStream *stream = static_cast<ScreenCastStream *>(data);
    struct spa_data *spa_data = buffer->buffer->datas;

    spa_data->mapoffset = 0;
    spa_data->flags = SPA_DATA_FLAG_READWRITE;

    if (spa_data[0].type != SPA_ID_INVALID && spa_data[0].type & (1 << SPA_DATA_DmaBuf))
        dmabuf.reset(kwinApp()->platform()->createDmaBufTexture(stream->m_resolution));

    if (dmabuf) {
      spa_data->type = SPA_DATA_DmaBuf;
      spa_data->fd = dmabuf->fd();
      spa_data->data = nullptr;
      spa_data->maxsize = dmabuf->stride() * stream->m_resolution.height();

      stream->m_dmabufDataForPwBuffer.insert(buffer, dmabuf);
#ifdef F_SEAL_SEAL //Disable memfd on systems that don't have it, like BSD < 12
    } else {
        if (!(spa_data[0].type & (1 << SPA_DATA_MemFd))) {
            qCCritical(KWIN_SCREENCAST) << "memfd: Client doesn't support memfd buffer data type";
            return;
        }

        const int bytesPerPixel = stream->m_source->hasAlphaChannel() ? 4 : 3;
        const int stride = SPA_ROUND_UP_N (stream->m_resolution.width() * bytesPerPixel, 4);
        spa_data->maxsize = stride * stream->m_resolution.height();
        spa_data->type = SPA_DATA_MemFd;
        spa_data->fd = memfd_create("kwin-screencast-memfd", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (spa_data->fd == -1) {
            qCCritical(KWIN_SCREENCAST) << "memfd: Can't create memfd";
            return;
        }
        spa_data->mapoffset = 0;

        if (ftruncate (spa_data->fd, spa_data->maxsize) < 0) {
            qCCritical(KWIN_SCREENCAST) << "memfd: Can't truncate to" << spa_data->maxsize;
            return;
        }

        unsigned int seals = F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
        if (fcntl(spa_data->fd, F_ADD_SEALS, seals) == -1)
            qCWarning(KWIN_SCREENCAST) << "memfd: Failed to add seals";

        spa_data->data = mmap(nullptr,
                              spa_data->maxsize,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED,
                              spa_data->fd,
                              spa_data->mapoffset);
        if (spa_data->data == MAP_FAILED)
            qCCritical(KWIN_SCREENCAST) << "memfd: Failed to mmap memory";
        else
            qCDebug(KWIN_SCREENCAST) << "memfd: created successfully" << spa_data->data << spa_data->maxsize;
#endif
    }
}

void ScreenCastStream::onStreamRemoveBuffer(void *data, pw_buffer *buffer)
{
    ScreenCastStream *stream = static_cast<ScreenCastStream *>(data);
    stream->m_dmabufDataForPwBuffer.remove(buffer);

    struct spa_buffer *spa_buffer = buffer->buffer;
    struct spa_data *spa_data = spa_buffer->datas;
    if (spa_data && spa_data->type == SPA_DATA_MemFd) {
        munmap (spa_data->data, spa_data->maxsize);
        close (spa_data->fd);
    }
}

ScreenCastStream::ScreenCastStream(ScreenCastSource *source, QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_resolution(source->textureSize())
{
    connect(source, &ScreenCastSource::closed, this, &ScreenCastStream::stopStreaming);

    pwStreamEvents.version = PW_VERSION_STREAM_EVENTS;
    pwStreamEvents.add_buffer = &ScreenCastStream::onStreamAddBuffer;
    pwStreamEvents.remove_buffer = &ScreenCastStream::onStreamRemoveBuffer;
    pwStreamEvents.state_changed = &ScreenCastStream::onStreamStateChanged;
    pwStreamEvents.param_changed = &ScreenCastStream::onStreamParamChanged;
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

    connect(pwCore.data(), &PipeWireCore::pipewireFailed, this, &ScreenCastStream::coreFailed);

    if (!createStream()) {
        qCWarning(KWIN_SCREENCAST) << "Failed to create PipeWire stream";
        m_error = i18n("Failed to create PipeWire stream");
        return false;
    }

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

    uint8_t buffer[2048];
    spa_pod_builder podBuilder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_fraction minFramerate = SPA_FRACTION(1, 1);
    spa_fraction maxFramerate = SPA_FRACTION(25, 1);
    spa_fraction defaultFramerate = SPA_FRACTION(0, 1);

    spa_rectangle resolution = SPA_RECTANGLE(uint32_t(m_resolution.width()), uint32_t(m_resolution.height()));

    // TODO[explicit_modifiers]: query modifiers supported/used by kwin
    uint64_t modifier = DRM_FORMAT_MOD_INVALID;

    const spa_pod *params[2];
    int n_params;

    auto canCreateDmaBuf = [this] () -> bool {
        return QSharedPointer<DmaBufTexture>(kwinApp()->platform()->createDmaBufTexture(m_resolution));
    };
    const auto format = m_source->hasAlphaChannel() ? SPA_VIDEO_FORMAT_BGRA : SPA_VIDEO_FORMAT_BGR;

    if (canCreateDmaBuf()) {
      params[0] = buildFormat(&podBuilder, SPA_VIDEO_FORMAT_BGRA, &resolution, &defaultFramerate, &minFramerate, &maxFramerate, &modifier, 1);
      params[1] = buildFormat(&podBuilder, format, &resolution, &defaultFramerate, &minFramerate, &maxFramerate, nullptr, 0);
      n_params = 2;
    } else {
      params[0] = buildFormat(&podBuilder, format, &resolution, &defaultFramerate, &minFramerate, &maxFramerate, nullptr, 0);
      n_params = 1;
    }

    pw_stream_add_listener(pwStream, &streamListener, &pwStreamEvents, this);
    auto flags = pw_stream_flags(PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_ALLOC_BUFFERS);

    if (pw_stream_connect(pwStream, PW_DIRECTION_OUTPUT, SPA_ID_INVALID, flags, params, n_params) != 0) {
        qCWarning(KWIN_SCREENCAST) << "Could not connect to stream";
        pw_stream_destroy(pwStream);
        pwStream = nullptr;
        return false;
    }

    if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded) {
        connect(Cursors::self(), &Cursors::positionChanged, this, [this] {
            if (auto scene = Compositor::self()->scene()) {
                scene->makeOpenGLContextCurrent();
            }
            recordFrame(QRegion{m_cursor.lastRect} | cursorGeometry(Cursors::self()->currentCursor()));
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

void ScreenCastStream::recordFrame(const QRegion &damagedRegion)
{
    Q_ASSERT(!m_stopped);

    if (m_pendingBuffer) {
        qCWarning(KWIN_SCREENCAST) << "Dropping a screencast frame because the compositor is slow";
        return;
    }

    if (m_source->textureSize() != m_resolution) {
        m_resolution = m_source->textureSize();
        newStreamParams();
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

    uint8_t *data = (uint8_t *) spa_data->data;
    if (!data && spa_buffer->datas->type != SPA_DATA_DmaBuf) {
        qCWarning(KWIN_SCREENCAST) << "Failed to record frame: invalid buffer data";
        pw_stream_queue_buffer(pwStream, buffer);
        return;
    }

    const auto size = m_source->textureSize();
    spa_data->chunk->offset = 0;
    if (data || spa_data[0].type == SPA_DATA_MemFd) {
        const bool hasAlpha = m_source->hasAlphaChannel();
        const int bpp = data && !hasAlpha ? 3 : 4;
        const uint stride = SPA_ROUND_UP_N (size.width() * bpp, 4);

        QImage dest(data, size.width(), size.height(), stride, hasAlpha ? QImage::Format_RGBA8888_Premultiplied : QImage::Format_RGB888);
        if (dest.sizeInBytes() > spa_data->maxsize) {
            qCDebug(KWIN_SCREENCAST) << "Failed to record frame: frame is too big";
            pw_stream_queue_buffer(pwStream, buffer);
            return;
        }

        spa_data->chunk->size = dest.sizeInBytes();
        spa_data->chunk->stride = dest.bytesPerLine();

        m_source->render(&dest);

        auto cursor = Cursors::self()->currentCursor();
        if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded && m_cursor.viewport.contains(cursor->pos())) {
            QPainter painter(&dest);
            const auto position = (cursor->pos() - m_cursor.viewport.topLeft() - cursor->hotspot()) * m_cursor.scale;
            painter.drawImage(QRect{position, cursor->image().size()}, cursor->image());
        }
    } else {
        auto &buf = m_dmabufDataForPwBuffer[buffer];

        spa_data->chunk->stride = buf->stride();
        spa_data->chunk->size = spa_data->maxsize;

        m_source->render(buf->framebuffer());

        auto cursor = Cursors::self()->currentCursor();
        if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded && m_cursor.viewport.contains(cursor->pos())) {
            GLRenderTarget::pushRenderTarget(buf->framebuffer());

            QRect r(QPoint(), size);
            auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

            QMatrix4x4 mvp;
            mvp.ortho(r);
            shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

            if (!m_cursor.texture || m_cursor.lastKey != cursor->image().cacheKey())
                m_cursor.texture.reset(new GLTexture(cursor->image()));

            m_cursor.texture->setYInverted(false);
            m_cursor.texture->bind();
            const auto cursorRect = cursorGeometry(cursor);
            mvp.translate(cursorRect.left(), r.height() - cursorRect.top() - cursor->image().height() * m_cursor.scale);
            shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            m_cursor.texture->render(cursorRect, cursorRect, true);
            glDisable(GL_BLEND);
            m_cursor.texture->unbind();
            m_cursor.lastRect = cursorRect;

            ShaderManager::instance()->popShader();
            GLRenderTarget::popRenderTarget();
        }
    }

    if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Metadata) {
        sendCursorData(Cursors::self()->currentCursor(),
                        (spa_meta_cursor *) spa_buffer_find_meta_data (spa_buffer, SPA_META_Cursor, sizeof (spa_meta_cursor)));
    }

    if (spa_meta *vdMeta = spa_buffer_find_meta(spa_buffer, SPA_META_VideoDamage)) {
        struct spa_meta_region *r = (spa_meta_region *) spa_meta_first(vdMeta);

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

    tryEnqueue(buffer);
}

void ScreenCastStream::recordCursor()
{
    Q_ASSERT(!m_stopped);

    if (m_pendingBuffer) {
        qCWarning(KWIN_SCREENCAST) << "Dropping a screencast cursor update because the compositor is slow";
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

    struct pw_buffer *buffer = pw_stream_dequeue_buffer(pwStream);

    if (!buffer) {
        return;
    }

    struct spa_buffer *spa_buffer = buffer->buffer;
    spa_buffer->datas[0].chunk->size = 0;
    sendCursorData(Cursors::self()->currentCursor(),
                   (spa_meta_cursor *) spa_buffer_find_meta_data (spa_buffer, SPA_META_Cursor, sizeof (spa_meta_cursor)));

    tryEnqueue(buffer);
}

void ScreenCastStream::tryEnqueue(pw_buffer *buffer)
{
    m_pendingBuffer = buffer;

    // The GPU doesn't necessarily process draw commands as soon as they are issued. Thus,
    // we need to insert a fence into the command stream and enqueue the pipewire buffer
    // only after the fence is signaled; otherwise stream consumers will most likely see
    // a corrupted buffer.
    if (kwinApp()->platform()->supportsNativeFence()) {
        Q_ASSERT_X(eglGetCurrentContext(), "tryEnqueue", "no current context");
        m_pendingFence = new EGLNativeFence(kwinApp()->platform()->sceneEglDisplay());
        if (!m_pendingFence->isValid()) {
            qCWarning(KWIN_SCREENCAST) << "Failed to create a native EGL fence";
            glFinish();
            enqueue();
        } else {
            m_pendingNotifier = new QSocketNotifier(m_pendingFence->fileDescriptor(),
                                                    QSocketNotifier::Read, this);
            connect(m_pendingNotifier, &QSocketNotifier::activated, this, &ScreenCastStream::enqueue);
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

    delete m_pendingFence;
    delete m_pendingNotifier;

    pw_stream_queue_buffer(pwStream, m_pendingBuffer);

    m_pendingBuffer = nullptr;
    m_pendingFence = nullptr;
    m_pendingNotifier = nullptr;
}

spa_pod *ScreenCastStream::buildFormat(struct spa_pod_builder *b, enum spa_video_format format, struct spa_rectangle *resolution,
             struct spa_fraction *defaultFramerate, struct spa_fraction *minFramerate, struct spa_fraction *maxFramerate,
             uint64_t *modifiers, int modifierCount)
{
    struct spa_pod_frame f[2];
    int i, c;

    spa_pod_builder_push_object(b, &f[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
    spa_pod_builder_add(b, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);

    /* format */
    if (format == SPA_VIDEO_FORMAT_BGRA) {
        /* announce equivalent format without alpha */
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(3, format, format, SPA_VIDEO_FORMAT_BGRx), 0);
    } else {
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);
    }
    /* modifiers */
    if (modifierCount > 0) {
        // build an enumeration of modifiers
        // TODO[explicit_modifiers]: Use SPA_POD_PROP_FLAG_DONT_FIXATE
        // needs seperate fixateFormat method.
        spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY);
        spa_pod_builder_push_choice(b, &f[1], SPA_CHOICE_Enum, 0);
        // modifiers from the array
        for (i = 0, c = 0; i < modifierCount; i++) {
            spa_pod_builder_long(b, modifiers[i]);
            if (c++ == 0) {
                spa_pod_builder_long(b, modifiers[i]);
            }
        }
        spa_pod_builder_pop(b, &f[1]);
    }

    /* frame size */
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(resolution), 0);

    /* variable framerate */
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(defaultFramerate), 0);

    /* maximal framerate */
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_maxFramerate,
        SPA_POD_CHOICE_RANGE_Fraction(
            SPA_POD_Fraction(maxFramerate),
            SPA_POD_Fraction(minFramerate),
            SPA_POD_Fraction(maxFramerate)),
      0);
    return (spa_pod*)spa_pod_builder_pop(b, &f[0]);
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
    if (!cursor || !spa_meta_cursor || !m_cursor.viewport.contains(cursor->pos())) {
        return;
    }

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
    spa_meta_cursor->bitmap_offset = sizeof (struct spa_meta_cursor);

    struct spa_meta_bitmap *spa_meta_bitmap = SPA_MEMBER (spa_meta_cursor,
                                                          spa_meta_cursor->bitmap_offset,
                                                          struct spa_meta_bitmap);

    uint8_t *bitmap_data = SPA_MEMBER (spa_meta_bitmap, spa_meta_bitmap->offset, uint8_t);
    QImage dest(bitmap_data, std::min(m_cursor.bitmapSize.width(), image.width()), std::min(m_cursor.bitmapSize.height(), image.height()), QImage::Format_RGBA8888_Premultiplied);
    dest.setDevicePixelRatio(m_cursor.scale);
    dest.fill(Qt::transparent);

    if (!image.isNull()) {
        QPainter painter(&dest);
        painter.drawImage(QPoint(), image);
    }

    spa_meta_bitmap->format = SPA_VIDEO_FORMAT_RGBA;
    spa_meta_bitmap->offset = sizeof (struct spa_meta_bitmap);
    spa_meta_bitmap->size.width = dest.width();
    spa_meta_bitmap->size.height = dest.height();
    spa_meta_bitmap->stride = dest.bytesPerLine();
}

void ScreenCastStream::setCursorMode(KWaylandServer::ScreencastV1Interface::CursorMode mode, qreal scale, const QRect &viewport)
{
    m_cursor.mode = mode;
    m_cursor.scale = scale;
    m_cursor.viewport = viewport;
}

} // namespace KWin
