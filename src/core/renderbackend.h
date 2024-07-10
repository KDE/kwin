/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rendertarget.h"
#include "effect/globals.h"
#include "utils/filedescriptor.h"

#include <QObject>
#include <QPointer>
#include <memory>

namespace KWin
{

class GraphicsBuffer;
class Output;
class OverlayWindow;
class OutputLayer;
class SurfacePixmap;
class SurfacePixmapX11;
class SurfaceTexture;
class PresentationFeedback;
class RenderLoop;
class DrmDevice;
class SyncTimeline;

class PresentationFeedback
{
public:
    explicit PresentationFeedback() = default;
    PresentationFeedback(const PresentationFeedback &copy) = delete;
    PresentationFeedback(PresentationFeedback &&move) = default;
    virtual ~PresentationFeedback() = default;

    virtual void presented(std::chrono::nanoseconds refreshCycleDuration, std::chrono::nanoseconds timestamp, PresentationMode mode) = 0;
};

struct RenderTimeSpan
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::time_point{std::chrono::nanoseconds::zero()};
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::time_point{std::chrono::nanoseconds::zero()};

    RenderTimeSpan operator|(const RenderTimeSpan &other) const;
};

class KWIN_EXPORT RenderTimeQuery
{
public:
    virtual ~RenderTimeQuery() = default;
    virtual std::optional<RenderTimeSpan> query() = 0;
};

class KWIN_EXPORT CpuRenderTimeQuery : public RenderTimeQuery
{
public:
    /**
     * marks the start of the query
     */
    explicit CpuRenderTimeQuery();

    void end();

    std::optional<RenderTimeSpan> query() override;

private:
    const std::chrono::steady_clock::time_point m_start;
    std::optional<std::chrono::steady_clock::time_point> m_end;
};

class KWIN_EXPORT OutputFrame
{
public:
    explicit OutputFrame(RenderLoop *loop, std::chrono::nanoseconds refreshDuration);
    ~OutputFrame();

    void presented(std::chrono::nanoseconds timestamp, PresentationMode mode);

    void addFeedback(std::unique_ptr<PresentationFeedback> &&feedback);

    void setContentType(ContentType type);
    std::optional<ContentType> contentType() const;

    void setPresentationMode(PresentationMode mode);
    PresentationMode presentationMode() const;

    void setDamage(const QRegion &region);
    QRegion damage() const;
    void addRenderTimeQuery(std::unique_ptr<RenderTimeQuery> &&query);

    std::chrono::steady_clock::time_point targetPageflipTime() const;
    std::chrono::nanoseconds refreshDuration() const;
    std::chrono::nanoseconds predictedRenderTime() const;

    std::optional<double> brightness() const;
    void setBrightness(double brightness);

    std::optional<double> artificialHdrHeadroom() const;
    void setArtificialHdrHeadroom(double edr);

private:
    std::optional<RenderTimeSpan> queryRenderTime() const;

    const QPointer<RenderLoop> m_loop;
    const std::chrono::nanoseconds m_refreshDuration;
    const std::chrono::steady_clock::time_point m_targetPageflipTime;
    const std::chrono::nanoseconds m_predictedRenderTime;
    std::vector<std::unique_ptr<PresentationFeedback>> m_feedbacks;
    std::optional<ContentType> m_contentType;
    PresentationMode m_presentationMode = PresentationMode::VSync;
    QRegion m_damage;
    std::vector<std::unique_ptr<RenderTimeQuery>> m_renderTimeQueries;
    bool m_presented = false;
    std::optional<double> m_brightness;
    std::optional<double> m_artificialHdrHeadroom;
};

/**
 * The RenderBackend class is the base class for all rendering backends.
 */
class KWIN_EXPORT RenderBackend : public QObject
{
    Q_OBJECT

public:
    virtual CompositingType compositingType() const = 0;
    virtual OverlayWindow *overlayWindow() const;

    virtual bool checkGraphicsReset();

    virtual OutputLayer *primaryLayer(Output *output) = 0;
    virtual OutputLayer *cursorLayer(Output *output);
    virtual bool present(Output *output, const std::shared_ptr<OutputFrame> &frame) = 0;
    virtual void repairPresentation(Output *output);

    virtual DrmDevice *drmDevice() const;

    virtual bool testImportBuffer(GraphicsBuffer *buffer);
    virtual QHash<uint32_t, QList<uint64_t>> supportedFormats() const;

    virtual std::unique_ptr<SurfaceTexture> createSurfaceTextureX11(SurfacePixmapX11 *pixmap);
    virtual std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmap *pixmap);
};

} // namespace KWin
