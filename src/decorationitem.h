/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "item.h"

namespace KDecoration2
{
class Decoration;
}

namespace KWin
{

class AbstractClient;
class Deleted;
class GLTexture;
class Toplevel;

namespace Decoration
{
class DecoratedClientImpl;
}

class KWIN_EXPORT DecorationRenderer : public QObject
{
    Q_OBJECT

public:
    virtual void render(const QRegion &region) = 0;
    virtual QSharedPointer<GLTexture> toOpenGLTexture() const;

    void invalidate();

    // TODO: Move damage tracking inside DecorationItem.
    QRegion damage() const;
    void addDamage(const QRegion &region);
    void resetDamage();

    static WindowQuadList buildQuads(Toplevel *window);

Q_SIGNALS:
    void damaged(const QRegion &region);
    void invalidated();

protected:
    explicit DecorationRenderer(Decoration::DecoratedClientImpl *client);

    Decoration::DecoratedClientImpl *client() const;

    bool areImageSizesDirty() const {
        return m_imageSizesDirty;
    }
    void resetImageSizesDirty() {
        m_imageSizesDirty = false;
    }
    QImage renderToImage(const QRect &geo);
    void renderToPainter(QPainter *painter, const QRect &rect);

private:
    QPointer<Decoration::DecoratedClientImpl> m_client;
    QRegion m_damage;
    bool m_imageSizesDirty;
};

/**
 * The DecorationItem class represents a server-side decoration.
 */
class KWIN_EXPORT DecorationItem : public Item
{
    Q_OBJECT

public:
    explicit DecorationItem(KDecoration2::Decoration *decoration, AbstractClient *window, Item *parent = nullptr);

    DecorationRenderer *renderer() const;

private Q_SLOTS:
    void handleFrameGeometryChanged();
    void handleWindowClosed(Toplevel *original, Deleted *deleted);

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;

private:
    Toplevel *m_window;
    QPointer<KDecoration2::Decoration> m_decoration;
    QScopedPointer<DecorationRenderer> m_renderer;
};

} // namespace KWin
