/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QWaylandClientExtensionTemplate>
#include <QWidget>
#include <QWindow>

#include <qpa/qplatformwindow_p.h>

#include "qwayland-kde-blur-v1.h"
#include "qwayland-wayland.h"

#include "utils/ramfile.h"

class Context;

class ShmBuffer
{
public:
    ShmBuffer(::wl_buffer *buffer, ::wl_shm_pool *pool)
        : m_buffer(buffer)
        , m_pool(pool)
    {
    }

    ~ShmBuffer()
    {
        wl_buffer_destroy(m_buffer);
        wl_shm_pool_destroy(m_pool);
    }

    ::wl_buffer *m_buffer;
    ::wl_shm_pool *m_pool;
};

class Shm : public QWaylandClientExtensionTemplate<Shm>, public QtWayland::wl_shm
{
public:
    Shm()
        : QWaylandClientExtensionTemplate<Shm>(2)
    {
    }

    ~Shm() override
    {
        if (isActive()) {
            if (wl_shm::version() >= WL_SHM_RELEASE_SINCE_VERSION) {
                release();
            }
        }
    }

    std::unique_ptr<ShmBuffer> createBuffer(const QImage &image)
    {
        if (image.format() != QImage::Format_Alpha8) {
            return nullptr;
        }

        KWin::RamFile storage("buffer", image.constBits(), image.sizeInBytes());
        if (!storage.isValid()) {
            return nullptr;
        }

        wl_shm_pool *pool = wl_shm_create_pool(object(), storage.fd(), storage.size());
        wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, image.width(), image.height(), image.bytesPerLine(), WL_SHM_FORMAT_R8);

        return std::make_unique<ShmBuffer>(buffer, pool);
    }
};

class BlurMaskV1 : public QtWayland::kde_blur_mask_v1
{
public:
    BlurMaskV1(::kde_blur_mask_v1 *handle)
        : kde_blur_mask_v1(handle)
    {
    }

    ~BlurMaskV1() override
    {
        destroy();
    }
};

class BlurManagerV1 : public QWaylandClientExtensionTemplate<BlurManagerV1>, public QtWayland::kde_blur_manager_v1
{
public:
    BlurManagerV1()
        : QWaylandClientExtensionTemplate(1)
    {
        initialize();
    }

    ~BlurManagerV1() override
    {
        if (isActive()) {
            destroy();
        }
    }

    std::unique_ptr<BlurMaskV1> createBlurMask()
    {
        if (!isActive()) {
            return nullptr;
        }
        return std::make_unique<BlurMaskV1>(get_blur_mask());
    }
};

class Context
{
public:
    Context()
        : shm(std::make_unique<Shm>())
        , blurManagerV1(std::make_unique<BlurManagerV1>())
    {
    }

    std::unique_ptr<Shm> shm;
    std::unique_ptr<BlurManagerV1> blurManagerV1;
};

class BlurSurfaceV1 : protected QtWayland::kde_blur_surface_v1
{
public:
    ~BlurSurfaceV1() override
    {
        destroy();
    }

    void setMask(const QImage &image, const QRect &center)
    {
        auto buffer = m_context->shm->createBuffer(image);
        auto mask = m_context->blurManagerV1->createBlurMask();
        mask->set_mask(buffer->m_buffer);
        mask->set_scale(std::round(image.devicePixelRatio() * 120));
        mask->set_center(center.x(), center.y(), center.width(), center.height());

        set_mask(mask->object());
    }

    void unsetMask()
    {
        unset_mask();
    }

    void setRegion(const QRegion &region)
    {
        if (auto waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
            wl_region *nativeRegion = wl_compositor_create_region(waylandApp->compositor());
            for (const QRect &rect : region) {
                wl_region_add(nativeRegion, rect.x(), rect.y(), rect.width(), rect.height());
            }

            set_shape(nativeRegion);
            wl_region_destroy(nativeRegion);
        }
    }

    void unsetRegion()
    {
        unset_shape();
    }

    static std::unique_ptr<BlurSurfaceV1> create(Context *context, wl_surface *surface)
    {
        if (!context->blurManagerV1->isActive()) {
            return nullptr;
        }

        auto blurSurface = std::make_unique<BlurSurfaceV1>();
        blurSurface->m_context = context;
        blurSurface->init(context->blurManagerV1->get_blur(surface));
        return blurSurface;
    }

private:
    Context *m_context;
};

class Blur : public QObject
{
public:
    Blur(Context *context, QWindow *window)
        : m_context(context)
    {
        window->create();

        if (auto waylandWindow = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
            if (waylandWindow->surface()) {
                m_surface = BlurSurfaceV1::create(m_context, waylandWindow->surface());
            }

            connect(waylandWindow, &QNativeInterface::Private::QWaylandWindow::surfaceCreated, this, [this, waylandWindow]() {
                if ((m_surface = BlurSurfaceV1::create(m_context, waylandWindow->surface()))) {
                    if (m_mask.has_value()) {
                        m_surface->setMask(m_mask.value(), m_maskCenter.value());
                    }
                    if (m_region.has_value()) {
                        m_surface->setRegion(m_region.value());
                    }
                }
            });

            connect(waylandWindow, &QNativeInterface::Private::QWaylandWindow::surfaceDestroyed, this, [this]() {
                m_surface.reset();
            });
        }
    }

    void setMask(const QImage &image, const QRect &center)
    {
        m_mask = image;
        m_maskCenter = center;
        if (m_surface) {
            m_surface->setMask(image, center);
        }
    }

    void unsetMask()
    {
        m_mask.reset();
        m_maskCenter.reset();
        if (m_surface) {
            m_surface->unsetMask();
        }
    }

    void setRegion(const QRegion &region)
    {
        m_region = region;
        if (m_surface) {
            m_surface->setRegion(region);
        }
    }

    void unsetRegion()
    {
        m_region.reset();
        if (m_surface) {
            m_surface->unsetRegion();
        }
    }

private:
    Context *m_context;
    std::unique_ptr<BlurSurfaceV1> m_surface;

    std::optional<QImage> m_mask;
    std::optional<QRect> m_maskCenter;
    std::optional<QRegion> m_region;
};

class BlurWidget : public QWidget
{
public:
    BlurWidget();

private:
    void setTranslucency(int alpha);
    void setBlur(const QRegion &region, const QImage &mask, const QRect &center);

    std::unique_ptr<Context> m_context;
    std::unique_ptr<Blur> m_blur;
};

static std::tuple<QImage, QRect> makeRoundedCornerMask(const QSize &size, qreal devicePixelRatio, int cornerRadius)
{
    QImage image(size * devicePixelRatio, QImage::Format_Alpha8);
    image.fill(Qt::transparent);
    image.setDevicePixelRatio(devicePixelRatio);

    QPainterPath path;
    path.addRoundedRect(0, 0, size.width(), size.height(), cornerRadius, cornerRadius);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillPath(path, Qt::black);
    painter.end();

    return {image, QRect(cornerRadius, cornerRadius, size.width() - 2 * cornerRadius, size.height() - 2 * cornerRadius)};
}

BlurWidget::BlurWidget()
    : m_context(std::make_unique<Context>())
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setTranslucency(128);

    resize(1000, 700);

    auto [mask, center] = makeRoundedCornerMask(QSize(50, 50), devicePixelRatio(), 10);
    setBlur(QRect(0, 0, 500, 350), mask, center);
}

void BlurWidget::setTranslucency(int alpha)
{
    QPalette p = palette();
    QColor windowColor = p.color(QPalette::Window);
    windowColor.setAlpha(alpha);
    p.setColor(QPalette::Window, windowColor);
    setPalette(p);
}

void BlurWidget::setBlur(const QRegion &region, const QImage &mask, const QRect &center)
{
    winId();

    if (!m_blur) {
        m_blur = std::make_unique<Blur>(m_context.get(), windowHandle());
    }

    m_blur->setRegion(region);
    m_blur->setMask(mask, center);
    update();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    BlurWidget widget;
    widget.show();

    return app.exec();
}
