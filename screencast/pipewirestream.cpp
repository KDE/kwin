/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "pipewirestream.h"
#include "cursor.h"
#include "dmabuftexture.h"
#include "kwingltexture.h"
#include "kwinglutils.h"
#include "kwinscreencast_logging.h"
#include "main.h"
#include "pipewirecore.h"
#include "platform.h"
#include "utils.h"

#include <KLocalizedString>

#include <QLoggingCategory>
#include <QPainter>

#include <spa/buffer/meta.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace KWin
{

void PipeWireStream::onStreamStateChanged(void *data, pw_stream_state old, pw_stream_state state, const char *error_message)
{
    PipeWireStream *pw = static_cast<PipeWireStream*>(data);
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

void PipeWireStream::newStreamParams()
{
    const int bpp = videoFormat.format == SPA_VIDEO_FORMAT_RGB || videoFormat.format == SPA_VIDEO_FORMAT_BGR ? 3 : 4;
    auto stride = SPA_ROUND_UP_N (m_resolution.width() * bpp, 4);

    uint8_t paramsBuffer[1024];
    spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT (paramsBuffer, sizeof (paramsBuffer));

    spa_rectangle resolution = SPA_RECTANGLE(uint32_t(m_resolution.width()), uint32_t(m_resolution.height()));
    const int cursorSize = Cursors::self()->currentCursor()->themeSize() * m_cursor.scale;
    const spa_pod *params[] = {
        (spa_pod*) spa_pod_builder_add_object(&pod_builder,
                                              SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                                              SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&resolution),
                                              SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(16, 2, 16),
                                              SPA_PARAM_BUFFERS_blocks, SPA_POD_Int (1),
                                              SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
                                              SPA_PARAM_BUFFERS_size, SPA_POD_Int(stride * m_resolution.height()),
                                              SPA_PARAM_BUFFERS_align, SPA_POD_Int(16)),
        (spa_pod*) spa_pod_builder_add_object (&pod_builder,
                                               SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                               SPA_PARAM_META_type, SPA_POD_Id (SPA_META_Cursor),
                                               SPA_PARAM_META_size, SPA_POD_Int (CURSOR_META_SIZE (cursorSize, cursorSize)))
    };
    pw_stream_update_params(pwStream, params, 2);
}

void PipeWireStream::onStreamParamChanged(void *data, uint32_t id, const struct spa_pod *format)
{
    if (!format || id != SPA_PARAM_Format) {
        return;
    }

    PipeWireStream *pw = static_cast<PipeWireStream *>(data);
    spa_format_video_raw_parse (format, &pw->videoFormat);
    qCDebug(KWIN_SCREENCAST) << "Stream format changed" << pw << pw->videoFormat.format;
    pw->newStreamParams();
}

void PipeWireStream::onStreamAddBuffer(void *data, pw_buffer *buffer)
{
    PipeWireStream *stream = static_cast<PipeWireStream *>(data);
    struct spa_data *spa_data = buffer->buffer->datas;

    spa_data->mapoffset = 0;
    spa_data->flags = SPA_DATA_FLAG_READWRITE;

    QSharedPointer<DmaBufTexture> dmabuf(kwinApp()->platform()->createDmaBufTexture(stream->m_resolution));
    if (dmabuf) {
      spa_data->type = SPA_DATA_DmaBuf;
      spa_data->fd = dmabuf->fd();
      spa_data->data = nullptr;
      spa_data->maxsize = dmabuf->stride() * stream->m_resolution.height();

      stream->m_dmabufDataForPwBuffer.insert(buffer, dmabuf);
#ifdef F_SEAL_SEAL //Disable memfd on systems that don't have it, like BSD < 12
    } else {
        const int bytesPerPixel = stream->m_hasAlpha ? 4 : 3;
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

void PipeWireStream::onStreamRemoveBuffer(void *data, pw_buffer *buffer)
{
    PipeWireStream *stream = static_cast<PipeWireStream *>(data);

    struct spa_buffer *spa_buffer = buffer->buffer;
    struct spa_data *spa_data = spa_buffer->datas;

    if (spa_data->type == SPA_DATA_DmaBuf) {
        stream->m_dmabufDataForPwBuffer.remove(buffer);
    } else if (spa_data->type == SPA_DATA_MemFd) {
        munmap (spa_data->data, spa_data->maxsize);
        close (spa_data->fd);
    }
}

PipeWireStream::PipeWireStream(bool hasAlpha, const QSize &resolution, QObject *parent)
    : QObject(parent)
    , m_resolution(resolution)
    , m_hasAlpha(hasAlpha)
{
    pwStreamEvents.version = PW_VERSION_STREAM_EVENTS;
    pwStreamEvents.add_buffer = &PipeWireStream::onStreamAddBuffer;
    pwStreamEvents.remove_buffer = &PipeWireStream::onStreamRemoveBuffer;
    pwStreamEvents.state_changed = &PipeWireStream::onStreamStateChanged;
    pwStreamEvents.param_changed = &PipeWireStream::onStreamParamChanged;
}

PipeWireStream::~PipeWireStream()
{
    m_stopped = true;
    if (pwStream) {
        pw_stream_destroy(pwStream);
    }
}

bool PipeWireStream::init()
{
    pwCore = PipeWireCore::self();
    if (!pwCore->m_error.isEmpty()) {
        m_error = pwCore->m_error;
        return false;
    }

    connect(pwCore.data(), &PipeWireCore::pipewireFailed, this, &PipeWireStream::coreFailed);

    if (!createStream()) {
        qCWarning(KWIN_SCREENCAST) << "Failed to create PipeWire stream";
        m_error = i18n("Failed to create PipeWire stream");
        return false;
    }

    return true;
}

uint PipeWireStream::framerate()
{
    if (pwStream) {
        return videoFormat.max_framerate.num / videoFormat.max_framerate.denom;
    }

    return 0;
}

uint PipeWireStream::nodeId()
{
    return pwNodeId;
}

bool PipeWireStream::createStream()
{
    const QByteArray objname = "kwin-screencast-" + objectName().toUtf8();
    pwStream = pw_stream_new(pwCore->pwCore, objname, nullptr);

    uint8_t buffer[1024];
    spa_pod_builder podBuilder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_fraction minFramerate = SPA_FRACTION(1, 1);
    spa_fraction maxFramerate = SPA_FRACTION(25, 1);
    spa_fraction defaultFramerate = SPA_FRACTION(0, 1);

    spa_rectangle resolution = SPA_RECTANGLE(uint32_t(m_resolution.width()), uint32_t(m_resolution.height()));

    auto canCreateDmaBuf = [this] () -> bool {
        return QSharedPointer<DmaBufTexture>(kwinApp()->platform()->createDmaBufTexture(m_resolution));
    };
    const auto format = m_hasAlpha || canCreateDmaBuf() ? SPA_VIDEO_FORMAT_BGRA : SPA_VIDEO_FORMAT_BGR;

    const spa_pod *param = (spa_pod*)spa_pod_builder_add_object(&podBuilder,
                                        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
                                        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
                                        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
                                        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(format == SPA_VIDEO_FORMAT_BGRA ? 3 : 2,
                                                                                        format, format, format == SPA_VIDEO_FORMAT_BGRA ? SPA_VIDEO_FORMAT_BGRx : SPA_VIDEO_FORMAT_UNKNOWN),
                                        SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&resolution),
                                        SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&defaultFramerate),
                                        SPA_FORMAT_VIDEO_maxFramerate, SPA_POD_CHOICE_RANGE_Fraction(&maxFramerate, &minFramerate, &maxFramerate));

    pw_stream_add_listener(pwStream, &streamListener, &pwStreamEvents, this);
    auto flags = pw_stream_flags(PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_ALLOC_BUFFERS);

    if (pw_stream_connect(pwStream, PW_DIRECTION_OUTPUT, SPA_ID_INVALID, flags, &param, 1) != 0) {
        qCWarning(KWIN_SCREENCAST) << "Could not connect to stream";
        pw_stream_destroy(pwStream);
        pwStream = nullptr;
        return false;
    }

    if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded) {
        connect(Cursors::self(), &Cursors::positionChanged, this, [this] {
            if (m_cursor.lastFrameTexture) {
                m_repainting = true;
                recordFrame(m_cursor.lastFrameTexture.data(), QRegion{m_cursor.lastRect} | cursorGeometry(Cursors::self()->currentCursor()));
                m_repainting = false;
            }
        });
    }

    return true;
}
void PipeWireStream::coreFailed(const QString &errorMessage)
{
    m_error = errorMessage;
    Q_EMIT stopStreaming();
}

void PipeWireStream::stop()
{
    m_stopped = true;
    delete this;
}

static GLTexture *copyTexture(GLTexture *texture)
{
    GLTexture *copy = new GLTexture(texture->internalFormat(), texture->size());
    copy->setFilter(GL_LINEAR);
    copy->setWrapMode(GL_CLAMP_TO_EDGE);

    const QRect r({}, texture->size());

    copy->bind();
    glCopyTextureSubImage2D(copy->texture(), 0, 0, 0, 0, 0, r.width(), r.height());
    copy->unbind();
    return copy;
}

void PipeWireStream::recordFrame(GLTexture *frameTexture, const QRegion &damagedRegion)
{
    Q_ASSERT(!m_stopped);
    Q_ASSERT(frameTexture);

    if (frameTexture->size() != m_resolution) {
        m_resolution = frameTexture->size();
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

    const auto size = frameTexture->size();
    spa_data->chunk->offset = 0;
    if (data) {
        const int bpp = data && !m_hasAlpha ? 3 : 4;
        const uint stride = SPA_ROUND_UP_N (size.width() * bpp, 4);
        const uint bufferSize = stride * size.height();

        if (bufferSize > spa_data->maxsize) {
            qCDebug(KWIN_SCREENCAST) << "Failed to record frame: frame is too big";
            pw_stream_queue_buffer(pwStream, buffer);
            return;
        }

        spa_data->chunk->size = bufferSize;
        spa_data->chunk->stride = stride;

        frameTexture->bind();
        glGetTextureImage(frameTexture->texture(), 0, m_hasAlpha ? GL_BGRA : GL_BGR, GL_UNSIGNED_BYTE, bufferSize, data);
        auto cursor = Cursors::self()->currentCursor();
        if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded && m_cursor.viewport.contains(cursor->pos())) {
            QImage dest(data, size.width(), size.height(), QImage::Format_RGBA8888_Premultiplied);
            QPainter painter(&dest);
            const auto position = (cursor->pos() - m_cursor.viewport.topLeft() - cursor->hotspot()) * m_cursor.scale;
            painter.drawImage(QRect{position, cursor->image().size()}, cursor->image());
        }
    } else {
        auto &buf = m_dmabufDataForPwBuffer[buffer];

        spa_data->chunk->stride = buf->stride();
        spa_data->chunk->size = spa_data->maxsize;

        GLRenderTarget::pushRenderTarget(buf->framebuffer());
        frameTexture->bind();

        QRect r(QPoint(), size);
        auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

        QMatrix4x4 mvp;
        mvp.ortho(r);
        shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

        QRegion dr = damagedRegion;
        if (m_cursor.texture) {
            dr |= m_cursor.lastRect;
        }

        frameTexture->render(damagedRegion, r, true);

        auto cursor = Cursors::self()->currentCursor();
        if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Embedded && m_cursor.viewport.contains(cursor->pos())) {
            if (!m_repainting) //We need to copy the last version of the stream to render the moved cursor on top
                m_cursor.lastFrameTexture.reset(copyTexture(frameTexture));

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
        }
        ShaderManager::instance()->popShader();

        GLRenderTarget::popRenderTarget();
    }
    frameTexture->unbind();

    if (m_cursor.mode == KWaylandServer::ScreencastV1Interface::Metadata) {
        sendCursorData(Cursors::self()->currentCursor(),
                        (spa_meta_cursor *) spa_buffer_find_meta_data (spa_buffer, SPA_META_Cursor, sizeof (spa_meta_cursor)));
    }

    pw_stream_queue_buffer(pwStream, buffer);
}

QRect PipeWireStream::cursorGeometry(Cursor *cursor) const
{
    const auto position = (cursor->pos() - m_cursor.viewport.topLeft() - cursor->hotspot()) * m_cursor.scale;
    return QRect{position, m_cursor.texture->size()};
}

void PipeWireStream::sendCursorData(Cursor *cursor, spa_meta_cursor *spa_meta_cursor)
{
    if (!cursor || !spa_meta_cursor) {
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
    spa_meta_bitmap->format = SPA_VIDEO_FORMAT_RGBA;
    spa_meta_bitmap->offset = sizeof (struct spa_meta_bitmap);

    uint8_t *bitmap_data = SPA_MEMBER (spa_meta_bitmap, spa_meta_bitmap->offset, uint8_t);
    const int bufferSideSize = Cursors::self()->currentCursor()->themeSize() * m_cursor.scale;
    QImage dest(bitmap_data, std::min(bufferSideSize, image.width()), std::min(bufferSideSize, image.height()), QImage::Format_RGBA8888_Premultiplied);
    spa_meta_bitmap->size.width = dest.width();
    spa_meta_bitmap->size.height = dest.height();
    spa_meta_bitmap->stride = dest.bytesPerLine();

    if (image.isNull()) {
        return;
    }

    dest.fill(Qt::transparent);
    QPainter painter(&dest);
    painter.drawImage(QPoint(), image);
}

void PipeWireStream::setCursorMode(KWaylandServer::ScreencastV1Interface::CursorMode mode, qreal scale, const QRect &viewport)
{
    m_cursor.mode = mode;
    m_cursor.scale = scale;
    m_cursor.viewport = viewport;
}

} // namespace KWin
