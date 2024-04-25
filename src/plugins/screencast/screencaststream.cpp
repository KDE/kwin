/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencaststream.h"
#include "compositor.h"
#include "core/drmdevice.h"
#include "core/graphicsbufferallocator.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "cursor.h"
#include "kwinscreencast_logging.h"
#include "main.h"
#include "opengl/glplatform.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "pipewirecore.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "scene/workspacescene.h"
#include "screencastbuffer.h"
#include "screencastsource.h"
#include "utils/drm_format_helper.h"

#include <KLocalizedString>

#include <QLoggingCategory>
#include <QPainter>

#include <libdrm/drm_fourcc.h>
#include <spa/buffer/meta.h>

namespace KWin
{

static spa_video_format drmFourCCToSpaVideoFormat(quint32 format)
{
    switch (format) {
    case DRM_FORMAT_ARGB8888:
        return SPA_VIDEO_FORMAT_BGRA;
    case DRM_FORMAT_XRGB8888:
        return SPA_VIDEO_FORMAT_BGRx;
    case DRM_FORMAT_RGBA8888:
        return SPA_VIDEO_FORMAT_ABGR;
    case DRM_FORMAT_RGBX8888:
        return SPA_VIDEO_FORMAT_xBGR;
    case DRM_FORMAT_ABGR8888:
        return SPA_VIDEO_FORMAT_RGBA;
    case DRM_FORMAT_XBGR8888:
        return SPA_VIDEO_FORMAT_RGBx;
    case DRM_FORMAT_BGRA8888:
        return SPA_VIDEO_FORMAT_ARGB;
    case DRM_FORMAT_BGRX8888:
        return SPA_VIDEO_FORMAT_xRGB;
    case DRM_FORMAT_NV12:
        return SPA_VIDEO_FORMAT_NV12;
    case DRM_FORMAT_RGB888:
        return SPA_VIDEO_FORMAT_BGR;
    case DRM_FORMAT_BGR888:
        return SPA_VIDEO_FORMAT_RGB;
    default:
        qCDebug(KWIN_SCREENCAST) << "unknown format" << format;
        return SPA_VIDEO_FORMAT_xRGB;
    }
}

void ScreenCastStream::onStreamStateChanged(pw_stream_state old, pw_stream_state state, const char *error_message)
{
    qCDebug(KWIN_SCREENCAST) << objectName() << "state changed" << pw_stream_state_as_string(old) << " -> " << pw_stream_state_as_string(state) << error_message;
    if (m_closed) {
        return;
    }

    m_streaming = false;

    switch (state) {
    case PW_STREAM_STATE_ERROR:
        qCWarning(KWIN_SCREENCAST) << objectName() << "Stream error: " << error_message;
        break;
    case PW_STREAM_STATE_PAUSED:
        if (nodeId() == 0 && m_pwStream) {
            m_pwNodeId = pw_stream_get_node_id(m_pwStream);
            Q_EMIT ready(nodeId());
        }
        m_source->pause();
        break;
    case PW_STREAM_STATE_STREAMING:
        m_streaming = true;
        m_lastSent = QDateTime();
        m_source->resume();
        break;
    case PW_STREAM_STATE_CONNECTING:
        break;
    case PW_STREAM_STATE_UNCONNECTED:
        close();
        break;
    }
}

#define CURSOR_BPP 4
#define CURSOR_META_SIZE(w, h) (sizeof(struct spa_meta_cursor) + sizeof(struct spa_meta_bitmap) + w * h * CURSOR_BPP)
static const int videoDamageRegionCount = 16;

void ScreenCastStream::newStreamParams()
{
    qCDebug(KWIN_SCREENCAST) << objectName() << "announcing stream params. with dmabuf:" << m_dmabufParams.has_value();
    uint8_t paramsBuffer[1024];
    spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT(paramsBuffer, sizeof(paramsBuffer));
    const int buffertypes = m_dmabufParams ? (1 << SPA_DATA_DmaBuf) : (1 << SPA_DATA_MemFd);
    const int bpp = m_videoFormat.format == SPA_VIDEO_FORMAT_RGB || m_videoFormat.format == SPA_VIDEO_FORMAT_BGR ? 3 : 4;
    const int stride = SPA_ROUND_UP_N(m_resolution.width() * bpp, 4);

    struct spa_pod_frame f;
    spa_pod_builder_push_object(&pod_builder, &f, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers);
    spa_pod_builder_add(&pod_builder,
                        SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(3, 2, 4),
                        SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(buffertypes), 0);
    if (!m_dmabufParams) {
        spa_pod_builder_add(&pod_builder,
                            SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
                            SPA_PARAM_BUFFERS_size, SPA_POD_Int(stride * m_resolution.height()),
                            SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
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

    pw_stream_update_params(m_pwStream, params.data(), params.count());
}

void ScreenCastStream::onStreamParamChanged(uint32_t id, const struct spa_pod *format)
{
    if (m_closed) {
        return;
    }

    if (!format || id != SPA_PARAM_Format) {
        qCDebug(KWIN_SCREENCAST) << objectName() << "stream param request ignored, id:" << id << "and with format:"<< (format != nullptr);
        return;
    }

    spa_format_video_raw_parse(format, &m_videoFormat);
    auto modifierProperty = spa_pod_find_prop(format, nullptr, SPA_FORMAT_VIDEO_modifier);
    QList<uint64_t> receivedModifiers;
    if (modifierProperty) {
        const struct spa_pod *modifierPod = &modifierProperty->value;

        uint32_t modifiersCount = SPA_POD_CHOICE_N_VALUES(modifierPod);
        uint64_t *modifiers = (uint64_t *)SPA_POD_CHOICE_VALUES(modifierPod);
        receivedModifiers = QList<uint64_t>(modifiers, modifiers + modifiersCount);
        // Remove duplicates
        std::sort(receivedModifiers.begin(), receivedModifiers.end());
        receivedModifiers.erase(std::unique(receivedModifiers.begin(), receivedModifiers.end()), receivedModifiers.end());

        if (!m_dmabufParams || !receivedModifiers.contains(m_dmabufParams->modifier)) {
            if (modifierProperty->flags & SPA_POD_PROP_FLAG_DONT_FIXATE) {
                // DRM_MOD_INVALID should be used as a last option. Do not just remove it it's the only
                // item on the list
                if (receivedModifiers.count() > 1) {
                    receivedModifiers.removeAll(DRM_FORMAT_MOD_INVALID);
                }
                m_dmabufParams = testCreateDmaBuf(m_resolution, m_drmFormat, receivedModifiers);
            } else {
                m_dmabufParams = testCreateDmaBuf(m_resolution, m_drmFormat, {DRM_FORMAT_MOD_INVALID});
            }

            // In case we fail to use any modifier from the list of offered ones, remove these
            // from our all future offerings, otherwise there will be no indication that it cannot
            // be used and clients can go for it over and over
            if (!m_dmabufParams.has_value()) {
                for (uint64_t modifier : receivedModifiers) {
                    m_modifiers.removeAll(modifier);
                }
            }

            qCDebug(KWIN_SCREENCAST) << objectName() << "Stream dmabuf modifiers received, offering our best suited modifier" << m_dmabufParams.has_value();
            char buffer[2048];
            auto params = buildFormats(m_dmabufParams.has_value(), buffer);
            pw_stream_update_params(m_pwStream, params.data(), params.count());
            return;
        }
    } else {
      m_dmabufParams.reset();
    }

    qCDebug(KWIN_SCREENCAST) << objectName() << "Stream format found, defining buffers";
    newStreamParams();
    m_streaming = true;
}

void ScreenCastStream::onStreamAddBuffer(pw_buffer *pwBuffer)
{
    if (m_closed) {
        return;
    }

    m_waitForNewBuffers = false;

    struct spa_data *spa_data = pwBuffer->buffer->datas;
    if (spa_data[0].type != SPA_ID_INVALID && spa_data[0].type & (1 << SPA_DATA_DmaBuf)) {
        Q_ASSERT(m_dmabufParams);
        if (auto dmabuf = DmaBufScreenCastBuffer::create(pwBuffer, GraphicsBufferOptions{
                                                                       .size = QSize(m_dmabufParams->width, m_dmabufParams->height),
                                                                       .format = m_dmabufParams->format,
                                                                       .modifiers = {m_dmabufParams->modifier},
                                                                   })) {
            pwBuffer->user_data = dmabuf;
            return;
        }
    }

    if (spa_data->type & (1 << SPA_DATA_MemFd)) {
        if (auto memfd = MemFdScreenCastBuffer::create(pwBuffer, GraphicsBufferOptions{
                                                                     .size = m_resolution,
                                                                     .format = m_drmFormat,
                                                                     .software = true,
                                                                 })) {
            pwBuffer->user_data = memfd;
            return;
        }
    }
}

void ScreenCastStream::onStreamRemoveBuffer(pw_buffer *pwBuffer)
{
    if (ScreenCastBuffer *buffer = static_cast<ScreenCastBuffer *>(pwBuffer->user_data)) {
        delete buffer;
        pwBuffer->user_data = nullptr;
    }
}

void ScreenCastStream::onStreamRenegotiateFormat(uint64_t)
{
    if (m_closed) {
        return;
    }

    m_streaming = false; // pause streaming as we wait for the renegotiation
    char buffer[2048];
    auto params = buildFormats(m_dmabufParams.has_value(), buffer);
    pw_stream_update_params(m_pwStream, params.data(), params.count());
}

ScreenCastStream::ScreenCastStream(ScreenCastSource *source, std::shared_ptr<PipeWireCore> pwCore, QObject *parent)
    : QObject(parent)
    , m_pwCore(pwCore)
    , m_source(source)
    , m_resolution(source->textureSize())
{
    connect(source, &ScreenCastSource::frame, this, &ScreenCastStream::recordFrame);
    connect(source, &ScreenCastSource::closed, this, &ScreenCastStream::close);

    m_pwStreamEvents.version = PW_VERSION_STREAM_EVENTS;
    m_pwStreamEvents.add_buffer = [](void *data, struct pw_buffer *buffer) {
        auto _this = static_cast<ScreenCastStream *>(data);
        _this->onStreamAddBuffer(buffer);
    };
    m_pwStreamEvents.remove_buffer = [](void *data, struct pw_buffer *buffer) {
        auto _this = static_cast<ScreenCastStream *>(data);
        _this->onStreamRemoveBuffer(buffer);
    };
    m_pwStreamEvents.state_changed = [](void *data, pw_stream_state old, pw_stream_state state, const char *error_message) {
        auto _this = static_cast<ScreenCastStream *>(data);
        _this->onStreamStateChanged(old, state, error_message);
    };
    m_pwStreamEvents.param_changed = [](void *data, uint32_t id, const struct spa_pod *param) {
        auto _this = static_cast<ScreenCastStream *>(data);
        _this->onStreamParamChanged(id, param);
    };

    m_pendingFrame.setSingleShot(true);
    connect(&m_pendingFrame, &QTimer::timeout, this, [this] {
        recordFrame(m_pendingDamages);
    });
}

ScreenCastStream::~ScreenCastStream()
{
    m_closed = true;

    if (m_pwRenegotiate) {
        pw_loop_destroy_source(m_pwCore->pwMainLoop, m_pwRenegotiate);
    }
    if (m_pwStream) {
        pw_stream_destroy(m_pwStream);
    }
}

bool ScreenCastStream::init()
{
    if (!m_pwCore->m_error.isEmpty()) {
        m_error = m_pwCore->m_error;
        return false;
    }

    connect(m_pwCore.get(), &PipeWireCore::pipewireFailed, this, &ScreenCastStream::coreFailed);

    if (!createStream()) {
        qCWarning(KWIN_SCREENCAST) << objectName() << "Failed to create PipeWire stream";
        m_error = i18n("Failed to create PipeWire stream");
        return false;
    }

    m_pwRenegotiate = pw_loop_add_event(
        m_pwCore->pwMainLoop, [](void *data, uint64_t format) {
            auto _this = static_cast<ScreenCastStream *>(data);
            _this->onStreamRenegotiateFormat(format);
        },
        this);

    return true;
}

uint ScreenCastStream::framerate()
{
    if (m_pwStream) {
        return m_videoFormat.max_framerate.num / m_videoFormat.max_framerate.denom;
    }

    return 0;
}

uint ScreenCastStream::nodeId()
{
    return m_pwNodeId;
}

bool ScreenCastStream::createStream()
{
    const QByteArray objname = "kwin-screencast-" + objectName().toUtf8();
    m_pwStream = pw_stream_new(m_pwCore->pwCore, objname, nullptr);

    const auto supported = Compositor::self()->backend()->supportedFormats();
    auto itModifiers = supported.constFind(m_source->drmFormat());

    // If the offered format is not available for dmabuf, prefer converting to another one than resorting to memfd
    if (itModifiers == supported.constEnd() && !supported.isEmpty()) {
        itModifiers = supported.constFind(DRM_FORMAT_ARGB8888);
        if (itModifiers != supported.constEnd()) {
            m_drmFormat = itModifiers.key();
        }
    }

    if (itModifiers == supported.constEnd()) {
        m_drmFormat = m_source->drmFormat();
        m_modifiers = {DRM_FORMAT_MOD_INVALID};
    } else {
        m_drmFormat = itModifiers.key();
        m_modifiers = *itModifiers;
        // Also support modifier-less DmaBufs
        m_modifiers += DRM_FORMAT_MOD_INVALID;
    }
    m_hasDmaBuf = testCreateDmaBuf(m_resolution, m_drmFormat, {DRM_FORMAT_MOD_INVALID}).has_value();

    char buffer[2048];
    QList<const spa_pod *> params = buildFormats(false, buffer);

    pw_stream_add_listener(m_pwStream, &m_streamListener, &m_pwStreamEvents, this);
    auto flags = pw_stream_flags(PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_ALLOC_BUFFERS);

    if (pw_stream_connect(m_pwStream, PW_DIRECTION_OUTPUT, SPA_ID_INVALID, flags, params.data(), params.count()) != 0) {
        qCWarning(KWIN_SCREENCAST) << objectName() << "Could not connect to stream";
        pw_stream_destroy(m_pwStream);
        m_pwStream = nullptr;
        return false;
    }

    if (m_cursor.mode == ScreencastV1Interface::Embedded) {
        m_cursor.changedConnection = connect(Cursors::self(), &Cursors::currentCursorChanged, this, &ScreenCastStream::invalidateCursor);
        m_cursor.positionChangedConnection = connect(Cursors::self(), &Cursors::positionChanged, this, [this] {
            recordFrame({});
        });
    } else if (m_cursor.mode == ScreencastV1Interface::Metadata) {
        m_cursor.changedConnection = connect(Cursors::self(), &Cursors::currentCursorChanged, this, &ScreenCastStream::invalidateCursor);
        m_cursor.positionChangedConnection = connect(Cursors::self(), &Cursors::positionChanged, this, &ScreenCastStream::recordCursor);
    }

    qCDebug(KWIN_SCREENCAST) << objectName() << "stream created, drm format:" << FormatInfo::drmFormatName(m_drmFormat) << "with DMA-BUF:" << m_hasDmaBuf;
    return true;
}
void ScreenCastStream::coreFailed(const QString &errorMessage)
{
    m_error = errorMessage;
    close();
}

void ScreenCastStream::close()
{
    if (m_closed) {
        return;
    }

    m_streaming = false;
    m_closed = true;
    m_pendingFrame.stop();

    disconnect(m_cursor.changedConnection);
    m_cursor.changedConnection = {};
    disconnect(m_cursor.positionChangedConnection);
    m_cursor.positionChangedConnection = {};

    m_source->pause();

    Q_EMIT closed();
}

void ScreenCastStream::recordFrame(const QRegion &_damagedRegion)
{
    QRegion damagedRegion = _damagedRegion;
    Q_ASSERT(!m_closed);

    if (!m_streaming) {
        m_pendingDamages += damagedRegion;
        return;
    }

    if (m_videoFormat.max_framerate.num != 0 && !m_lastSent.isNull()) {
        auto frameInterval = (1000. * m_videoFormat.max_framerate.denom / m_videoFormat.max_framerate.num);
        auto lastSentAgo = m_lastSent.msecsTo(QDateTime::currentDateTimeUtc());
        if (lastSentAgo < frameInterval) {
            m_pendingDamages += damagedRegion;
            if (!m_pendingFrame.isActive()) {
                m_pendingFrame.start(frameInterval - lastSentAgo);
            }
            return;
        }
    }

    m_pendingDamages = {};

    if (m_waitForNewBuffers) {
        qCWarning(KWIN_SCREENCAST) << objectName() << "Waiting for new buffers to be created";
        return;
    }

    const auto size = m_source->textureSize();
    if (size != m_resolution) {
        m_resolution = size;
        m_waitForNewBuffers = true;
        m_dmabufParams = std::nullopt;
        pw_loop_signal_event(m_pwCore->pwMainLoop, m_pwRenegotiate);
        return;
    }

    const char *error = "";
    auto state = pw_stream_get_state(m_pwStream, &error);
    if (state != PW_STREAM_STATE_STREAMING) {
        if (error) {
            qCWarning(KWIN_SCREENCAST) << objectName() << "Failed to record frame: stream is not active" << error;
        }
        return;
    }

    struct pw_buffer *pwBuffer = pw_stream_dequeue_buffer(m_pwStream);
    if (!pwBuffer) {
        return;
    }

    struct spa_buffer *spa_buffer = pwBuffer->buffer;
    struct spa_data *spa_data = spa_buffer->datas;

    ScreenCastBuffer *buffer = static_cast<ScreenCastBuffer *>(pwBuffer->user_data);
    if (!buffer) {
        qCWarning(KWIN_SCREENCAST) << objectName() << "Failed to record frame: invalid buffer type";
        corruptHeader(spa_buffer);
        pw_stream_queue_buffer(m_pwStream, pwBuffer);
        return;
    }

    EglContext *context = static_cast<AbstractEglBackend *>(Compositor::self()->backend())->openglContext();
    context->makeCurrent();

    spa_data->chunk->flags = SPA_CHUNK_FLAG_NONE;
    if (auto memfd = dynamic_cast<MemFdScreenCastBuffer *>(buffer)) {
        m_source->render(memfd->view.image());

        auto cursor = Cursors::self()->currentCursor();
        if (m_cursor.mode == ScreencastV1Interface::Embedded && includesCursor(cursor)) {
            QPainter painter(memfd->view.image());
            const auto position = (cursor->pos() - m_cursor.viewport.topLeft() - cursor->hotspot()) * m_cursor.scale;
            const PlatformCursorImage cursorImage = kwinApp()->cursorImage();
            painter.drawImage(QRect{position.toPoint(), cursorImage.image().size()}, cursorImage.image());
        }
    } else if (auto dmabuf = dynamic_cast<DmaBufScreenCastBuffer *>(buffer)) {
        m_source->render(dmabuf->framebuffer.get());

        auto cursor = Cursors::self()->currentCursor();
        if (m_cursor.mode == ScreencastV1Interface::Embedded && includesCursor(cursor)) {
            if (m_cursor.invalid) {
                m_cursor.invalid = false;
                const PlatformCursorImage cursorImage = kwinApp()->cursorImage();
                if (cursorImage.isNull()) {
                    m_cursor.texture = nullptr;
                } else {
                    m_cursor.texture = GLTexture::upload(cursorImage.image());
                }
            }
            if (m_cursor.texture) {
                GLFramebuffer::pushFramebuffer(dmabuf->framebuffer.get());

                auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

                const QRectF cursorRect = scaledRect(cursor->geometry().translated(-m_cursor.viewport.topLeft()), m_cursor.scale);
                QMatrix4x4 mvp;
                mvp.scale(1, -1);
                mvp.ortho(QRectF(QPointF(0, 0), size));
                mvp.translate(cursorRect.x(), cursorRect.y());
                shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                m_cursor.texture->render(cursorRect.size());
                glDisable(GL_BLEND);

                ShaderManager::instance()->popShader();
                GLFramebuffer::popFramebuffer();

                damagedRegion += QRegion{m_cursor.lastRect.toAlignedRect()} | cursorRect.toAlignedRect();
                m_cursor.lastRect = cursorRect;
            } else {
                damagedRegion += m_cursor.lastRect.toAlignedRect();
                m_cursor.lastRect = {};
            }
        }

        // Implicit sync is broken on Nvidia.
        if (context->glPlatform()->isNvidia()) {
            glFinish();
        } else {
            glFlush();
        }
    }

    if (m_cursor.mode == ScreencastV1Interface::Metadata) {
        addCursorMetadata(spa_buffer, Cursors::self()->currentCursor());
    }

    addDamage(spa_buffer, damagedRegion);
    addHeader(spa_buffer);
    enqueue(pwBuffer);
}

void ScreenCastStream::addHeader(spa_buffer *spaBuffer)
{
    spa_meta_header *spaHeader = (spa_meta_header *)spa_buffer_find_meta_data(spaBuffer, SPA_META_Header, sizeof(spa_meta_header));
    if (spaHeader) {
        spaHeader->flags = 0;
        spaHeader->dts_offset = 0;
        spaHeader->seq = m_sequential++;
        spaHeader->pts = m_source->clock().count();
    }
}

void ScreenCastStream::corruptHeader(spa_buffer *spaBuffer)
{
    spa_meta_header *spaHeader = (spa_meta_header *)spa_buffer_find_meta_data(spaBuffer, SPA_META_Header, sizeof(spa_meta_header));
    if (spaHeader) {
        spaHeader->flags = SPA_META_HEADER_FLAG_CORRUPTED;
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

void ScreenCastStream::invalidateCursor()
{
    m_cursor.invalid = true;
}

void ScreenCastStream::recordCursor()
{
    Q_ASSERT(!m_closed);
    if (!m_streaming) {
        return;
    }

    const char *error = "";
    auto state = pw_stream_get_state(m_pwStream, &error);
    if (state != PW_STREAM_STATE_STREAMING) {
        if (error) {
            qCWarning(KWIN_SCREENCAST) << objectName() << "Failed to record cursor position: stream is not active" << error;
        }
        return;
    }

    if (!includesCursor(Cursors::self()->currentCursor()) && !m_cursor.visible) {
        return;
    }

    pw_buffer *pwBuffer = pw_stream_dequeue_buffer(m_pwStream);
    if (!pwBuffer) {
        return;
    }

    struct spa_buffer *spa_buffer = pwBuffer->buffer;

    // in pipewire terms, corrupted means "do not look at the frame contents" and here they're empty.
    spa_buffer->datas[0].chunk->flags = SPA_CHUNK_FLAG_CORRUPTED;

    addCursorMetadata(spa_buffer, Cursors::self()->currentCursor());
    addHeader(spa_buffer);
    addDamage(spa_buffer, {});
    enqueue(pwBuffer);
}

void ScreenCastStream::enqueue(pw_buffer *pwBuffer)
{
    pw_stream_queue_buffer(m_pwStream, pwBuffer);

    if (pwBuffer->buffer->datas[0].chunk->flags != SPA_CHUNK_FLAG_CORRUPTED) {
        m_lastSent = QDateTime::currentDateTimeUtc();
    }
}

QList<const spa_pod *> ScreenCastStream::buildFormats(bool fixate, char buffer[2048])
{
    const auto format = drmFourCCToSpaVideoFormat(m_drmFormat);
    spa_pod_builder podBuilder = SPA_POD_BUILDER_INIT(buffer, 2048);
    spa_fraction defFramerate = SPA_FRACTION(0, 1);
    spa_fraction minFramerate = SPA_FRACTION(1, 1);
    spa_fraction maxFramerate = SPA_FRACTION(m_source->refreshRate() / 1000, 1);

    spa_rectangle resolution = SPA_RECTANGLE(uint32_t(m_resolution.width()), uint32_t(m_resolution.height()));

    QList<const spa_pod *> params;
    params.reserve(fixate + m_hasDmaBuf + 1);
    if (fixate) {
        params.append(buildFormat(&podBuilder, SPA_VIDEO_FORMAT_BGRA, &resolution, &defFramerate, &minFramerate, &maxFramerate, {m_dmabufParams->modifier}, SPA_POD_PROP_FLAG_MANDATORY));
    }
    if (m_hasDmaBuf) {
        params.append(buildFormat(&podBuilder, SPA_VIDEO_FORMAT_BGRA, &resolution, &defFramerate, &minFramerate, &maxFramerate, m_modifiers, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE));
    }
    params.append(buildFormat(&podBuilder, format, &resolution, &defFramerate, &minFramerate, &maxFramerate, {}, 0));
    return params;
}

spa_pod *ScreenCastStream::buildFormat(struct spa_pod_builder *b, enum spa_video_format format, struct spa_rectangle *resolution,
                                       struct spa_fraction *defaultFramerate, struct spa_fraction *minFramerate, struct spa_fraction *maxFramerate,
                                       const QList<uint64_t> &modifiers, quint32 modifiersFlags)
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
    } else if (format == SPA_VIDEO_FORMAT_RGBA) {
        /* announce equivalent format without alpha */
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(3, format, format, SPA_VIDEO_FORMAT_RGBx), 0);
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

bool ScreenCastStream::includesCursor(Cursor *cursor) const
{
    if (Cursors::self()->isCursorHidden()) {
        return false;
    }
    return m_cursor.viewport.intersects(cursor->geometry());
}

void ScreenCastStream::addCursorMetadata(spa_buffer *spaBuffer, Cursor *cursor)
{
    if (!cursor) {
        return;
    }

    auto spaMetaCursor = (spa_meta_cursor *)spa_buffer_find_meta_data(spaBuffer, SPA_META_Cursor, sizeof(spa_meta_cursor));
    if (!spaMetaCursor) {
        return;
    }

    if (!includesCursor(cursor)) {
        spaMetaCursor->id = 0;
        spaMetaCursor->position.x = -1;
        spaMetaCursor->position.y = -1;
        spaMetaCursor->hotspot.x = -1;
        spaMetaCursor->hotspot.y = -1;
        spaMetaCursor->bitmap_offset = 0;
        m_cursor.visible = false;
        return;
    }
    m_cursor.visible = true;
    const auto position = (cursor->pos() - m_cursor.viewport.topLeft()) * m_cursor.scale;

    spaMetaCursor->id = 1;
    spaMetaCursor->position.x = position.x();
    spaMetaCursor->position.y = position.y();
    spaMetaCursor->hotspot.x = cursor->hotspot().x() * m_cursor.scale;
    spaMetaCursor->hotspot.y = cursor->hotspot().y() * m_cursor.scale;
    spaMetaCursor->bitmap_offset = 0;

    if (!m_cursor.invalid) {
        return;
    }

    m_cursor.invalid = false;
    spaMetaCursor->bitmap_offset = sizeof(struct spa_meta_cursor);

    const QSize targetSize = (cursor->rect().size() * m_cursor.scale).toSize();

    struct spa_meta_bitmap *spaMetaBitmap = SPA_MEMBER(spaMetaCursor,
                                                       spaMetaCursor->bitmap_offset,
                                                       struct spa_meta_bitmap);
    spaMetaBitmap->format = SPA_VIDEO_FORMAT_RGBA;
    spaMetaBitmap->offset = sizeof(struct spa_meta_bitmap);
    spaMetaBitmap->size.width = std::min(m_cursor.bitmapSize.width(), targetSize.width());
    spaMetaBitmap->size.height = std::min(m_cursor.bitmapSize.height(), targetSize.height());
    spaMetaBitmap->stride = spaMetaBitmap->size.width * 4;

    uint8_t *bitmap_data = SPA_MEMBER(spaMetaBitmap, spaMetaBitmap->offset, uint8_t);
    QImage dest(bitmap_data,
                spaMetaBitmap->size.width,
                spaMetaBitmap->size.height,
                spaMetaBitmap->stride,
                QImage::Format_RGBA8888_Premultiplied);
    dest.fill(Qt::transparent);

    const QImage image = kwinApp()->cursorImage().image();
    if (!image.isNull()) {
        QPainter painter(&dest);
        painter.drawImage(QRect({0, 0}, targetSize), image);
    }
}

void ScreenCastStream::setCursorMode(ScreencastV1Interface::CursorMode mode, qreal scale, const QRectF &viewport)
{
    m_cursor.mode = mode;
    m_cursor.scale = scale;
    m_cursor.viewport = viewport;
}

std::optional<ScreenCastDmaBufTextureParams> ScreenCastStream::testCreateDmaBuf(const QSize &size, quint32 format, const QList<uint64_t> &modifiers)
{
    AbstractEglBackend *backend = dynamic_cast<AbstractEglBackend *>(Compositor::self()->backend());
    if (!backend) {
        return std::nullopt;
    }

    GraphicsBuffer *buffer = backend->drmDevice()->allocator()->allocate(GraphicsBufferOptions{
        .size = size,
        .format = format,
        .modifiers = modifiers,
    });
    if (!buffer) {
        return std::nullopt;
    }
    auto drop = qScopeGuard([&buffer]() {
        buffer->drop();
    });

    const DmaBufAttributes *attrs = buffer->dmabufAttributes();
    if (!attrs) {
        return std::nullopt;
    }

    return ScreenCastDmaBufTextureParams{
        .planeCount = attrs->planeCount,
        .width = attrs->width,
        .height = attrs->height,
        .format = attrs->format,
        .modifier = attrs->modifier,
    };
}

} // namespace KWin

#include "moc_screencaststream.cpp"
