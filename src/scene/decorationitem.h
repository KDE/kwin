/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KDecoration2
{
class Decoration;
}

namespace KWin
{

class Window;
class Deleted;
class Output;

namespace Decoration
{
class DecoratedClientImpl;
}

class KWIN_EXPORT DecorationRenderer : public QObject
{
    Q_OBJECT

public:
    virtual void render(const QRegion &region) = 0;
    void invalidate();

    // TODO: Move damage tracking inside DecorationItem.
    QRegion damage() const;
    void addDamage(const QRegion &region);
    void resetDamage();

    qreal effectiveDevicePixelRatio() const;
    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal dpr);

    // Reserve some space for padding. We pad decoration parts to avoid texture bleeding.
    static const int TexturePad = 1;

Q_SIGNALS:
    void damaged(const QRegion &region);

protected:
    explicit DecorationRenderer(Decoration::DecoratedClientImpl *client);

    Decoration::DecoratedClientImpl *client() const;

    bool areImageSizesDirty() const
    {
        return m_imageSizesDirty;
    }
    void resetImageSizesDirty()
    {
        m_imageSizesDirty = false;
    }
    QImage renderToImage(const QRect &geo);
    void renderToPainter(QPainter *painter, const QRect &rect);

private:
    QPointer<Decoration::DecoratedClientImpl> m_client;
    QRegion m_damage;
    qreal m_devicePixelRatio = 1;
    bool m_imageSizesDirty;
};

/**
 * The DecorationItem class represents a server-side decoration.
 */
class KWIN_EXPORT DecorationItem : public Item
{
    Q_OBJECT

public:
    explicit DecorationItem(KDecoration2::Decoration *decoration, Window *window, Scene *scene, Item *parent = nullptr);

    DecorationRenderer *renderer() const;
    Window *window() const;

    QVector<QRectF> shape() const override final;
    QRegion opaque() const override final;

private Q_SLOTS:
    void handleFrameGeometryChanged();
    void handleWindowClosed(Window *original, Deleted *deleted);
    void handleOutputChanged();
    void handleOutputScaleChanged();

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;

private:
    Window *m_window;
    QPointer<Output> m_output;
    QPointer<KDecoration2::Decoration> m_decoration;
    std::unique_ptr<DecorationRenderer> m_renderer;
};

} // namespace KWin
