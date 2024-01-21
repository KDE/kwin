/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020-2021 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QSharedPointer>
#include <QSize>
#include <optional>

#include <pipewire/pipewire.h>
#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>

#undef Status

class PipeWireCore;
struct gbm_device;

typedef void *EGLDisplay;

struct DmaBufPlane
{
    int fd; ///< The dmabuf file descriptor
    uint32_t offset; ///< The offset from the start of buffer
    uint32_t stride; ///< The distance from the start of a row to the next row in bytes
};

struct DmaBufAttributes
{
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0; ///< The layout modifier

    QList<DmaBufPlane> planes;
};

struct PipeWireCursor
{
    QPoint position;
    QPoint hotspot;
    QImage texture;
    bool operator!=(const PipeWireCursor &other) const
    {
        return !operator==(other);
    };
    bool operator==(const PipeWireCursor &other) const
    {
        return position == other.position && hotspot == other.hotspot && texture == other.texture;
    }
};

struct PipeWireFrame
{
    spa_video_format format;
    std::optional<int> sequential;
    std::optional<std::chrono::nanoseconds> presentationTimestamp;
    std::optional<DmaBufAttributes> dmabuf;
    std::optional<QImage> image;
    std::optional<QRegion> damage;
    std::optional<PipeWireCursor> cursor;
};

struct Fraction
{
    bool operator==(const Fraction &other) const
    {
        return numerator == other.numerator && denominator == other.denominator;
    }
    explicit operator bool() const
    {
        return isValid();
    }
    bool isValid() const
    {
        return denominator > 0;
    }
    quint32 numerator = 0;
    quint32 denominator = 0;
};

QImage::Format SpaToQImageFormat(quint32 /*spa_video_format*/ format);

struct PipeWireSourceStreamPrivate;

class PipeWireSourceStream : public QObject
{
    Q_OBJECT
public:
    explicit PipeWireSourceStream(QObject *parent = nullptr);
    ~PipeWireSourceStream();

    Fraction framerate() const;
    void setMaxFramerate(const Fraction &framerate);
    uint nodeId();
    QString error() const;

    QSize size() const;
    pw_stream_state state() const;
    bool createStream(uint nodeid, int fd);
    void setActive(bool active);
    void setDamageEnabled(bool withDamage);

    void handleFrame(struct pw_buffer *buffer);
    void process();
    void renegotiateModifierFailed(spa_video_format format, quint64 modifier);

    std::optional<std::chrono::nanoseconds> currentPresentationTimestamp() const;

    static uint32_t spaVideoFormatToDrmFormat(spa_video_format spa_format);

    bool usingDmaBuf() const;
    bool allowDmaBuf() const;
    void setAllowDmaBuf(bool allowed);

Q_SIGNALS:
    void streamReady();
    void startStreaming();
    void stopStreaming();
    void streamParametersChanged();
    void frameReceived(const PipeWireFrame &frame);
    void stateChanged(pw_stream_state state, pw_stream_state oldState);

private:
    static void onStreamParamChanged(void *data, uint32_t id, const struct spa_pod *format);
    static void onStreamStateChanged(void *data, pw_stream_state old, pw_stream_state state, const char *error_message);
    static void onRenegotiate(void *data, uint64_t);
    QList<const spa_pod *> createFormatsParams(spa_pod_builder podBuilder);

    void coreFailed(const QString &errorMessage);
    QScopedPointer<PipeWireSourceStreamPrivate> d;
};
