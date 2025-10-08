/*
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/graphicsbuffer.h"

#include <QHash>
#include <QSet>
#include <sys/types.h>
#include <wayland-server.h>

namespace KWin
{

class Display;
class LinuxDmaBufV1ClientBufferIntegrationPrivate;
class LinuxDmaBufV1FeedbackPrivate;
class RenderBackend;
class DrmDevice;

class KWIN_EXPORT LinuxDmaBufV1Feedback : public QObject
{
    Q_OBJECT
public:
    ~LinuxDmaBufV1Feedback() override;

    enum class TrancheFlag : uint32_t {
        Scanout = 1,
        Sampling = 2,
    };
    Q_DECLARE_FLAGS(TrancheFlags, TrancheFlag)

    struct Tranche
    {
        std::shared_ptr<DrmDevice> device;
        TrancheFlags flags;
        QHash<uint32_t, QList<uint64_t>> formatTable;
    };
    /**
     * Sets the list of tranches for this feedback object, with lower indices
     * indicating a higher priority / a more optimal configuration.
     * The main device does not need to be included
     */
    void setScanoutTranches(DrmDevice *device, const QHash<uint32_t, QList<uint64_t>> &formats);
    void setTranches(const QList<Tranche> &tranches);

private:
    static QList<Tranche> createScanoutTranches(const QList<Tranche> &tranches, const std::shared_ptr<DrmDevice> &device, const QHash<uint32_t, QList<uint64_t>> &formats);

    LinuxDmaBufV1Feedback(LinuxDmaBufV1ClientBufferIntegrationPrivate *integration);
    friend class LinuxDmaBufV1ClientBufferIntegrationPrivate;
    friend class LinuxDmaBufV1FeedbackPrivate;
    std::unique_ptr<LinuxDmaBufV1FeedbackPrivate> d;
};

/**
 * The LinuxDmaBufV1ClientBufferIntegration class provides support for linux dma-buf buffers.
 */
class KWIN_EXPORT LinuxDmaBufV1ClientBufferIntegration : public QObject
{
    Q_OBJECT

public:
    explicit LinuxDmaBufV1ClientBufferIntegration(Display *display);
    ~LinuxDmaBufV1ClientBufferIntegration() override;

    RenderBackend *renderBackend() const;
    void setRenderBackend(RenderBackend *renderBackend);
    std::shared_ptr<DrmDevice> mainDevice() const;

    void setSupportedFormatsWithModifiers(const QList<LinuxDmaBufV1Feedback::Tranche> &tranches);

private:
    friend class LinuxDmaBufV1ClientBufferIntegrationPrivate;
    std::unique_ptr<LinuxDmaBufV1ClientBufferIntegrationPrivate> d;
};

} // namespace KWin
