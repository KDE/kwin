/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenedgeeffect.h"
// KWin
#include "effect/effecthandler.h"
#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"
// KDE
#include <KConfigGroup>
#include <KSharedConfig>
#include <KSvg/Svg>
// Qt
#include <QFile>
#include <QPainter>
#include <QTimer>

namespace KWin
{

ScreenEdgeEffect::ScreenEdgeEffect()
    : Effect()
    , m_cleanupTimer(new QTimer(this))
{
    connect(effects, &EffectsHandler::screenEdgeApproaching, this, &ScreenEdgeEffect::edgeApproaching);
    m_cleanupTimer->setInterval(5000);
    m_cleanupTimer->setSingleShot(true);
    connect(m_cleanupTimer, &QTimer::timeout, this, &ScreenEdgeEffect::cleanup);
    connect(effects, &EffectsHandler::screenLockingChanged, this, [this](bool locked) {
        if (locked) {
            cleanup();
        }
    });
}

ScreenEdgeEffect::~ScreenEdgeEffect()
{
    cleanup();
}

void ScreenEdgeEffect::ensureGlowSvg()
{
    if (!m_glow) {
        m_glow = new KSvg::Svg(this);
        m_glow->imageSet()->setBasePath(QStringLiteral("plasma/desktoptheme"));

        const QString groupName = QStringLiteral("Theme");
        KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("plasmarc"));
        KConfigGroup cg = KConfigGroup(config, groupName);
        m_glow->imageSet()->setImageSetName(cg.readEntry("name", QStringLiteral("default")));

        m_configWatcher = KConfigWatcher::create(config);

        connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group, const QByteArrayList &names) {
            if (group.name() != QStringLiteral("Theme") || !names.contains(QStringLiteral("name"))) {
                return;
            }
            m_glow->imageSet()->setImageSetName(group.readEntry("name", QStringLiteral("default")));
        });

        m_glow->setImagePath(QStringLiteral("widgets/glowbar"));
    }
}

void ScreenEdgeEffect::cleanup()
{
    m_glowItems.clear();
}

void ScreenEdgeEffect::edgeApproaching(ElectricBorder border, qreal factor, const QRect &geometry)
{
    auto it = m_glowItems.find(border);
    if (it != m_glowItems.end()) {
        ImageItem *glowItem = it->second.get();
        glowItem->setOpacity(factor);
        if (glowItem->position() != geometry.topLeft() || glowItem->size() != geometry.size()) {
            glowItem->setImage(glowImage(border, geometry.size()));
            glowItem->setPosition(geometry.topLeft());
            glowItem->setSize(geometry.size());
        }
        if (factor == 0.0) {
            m_cleanupTimer->start();
        } else {
            m_cleanupTimer->stop();
        }
    } else if (factor != 0.0) {
        std::unique_ptr<ImageItem> glow = createGlowItem(border, factor, geometry);
        if (glow) {
            m_glowItems[border] = std::move(glow);
        }
    }
}

std::unique_ptr<ImageItem> ScreenEdgeEffect::createGlowItem(ElectricBorder border, qreal factor, const QRect &geometry)
{
    const QImage image = glowImage(border, geometry.size());
    if (image.isNull()) {
        return nullptr;
    }

    WorkspaceScene *scene = effects->scene();

    std::unique_ptr<ImageItem> imageItem = scene->renderer()->createImageItem(scene, scene->overlayItem());
    imageItem->setImage(image);
    imageItem->setPosition(geometry.topLeft());
    imageItem->setSize(geometry.size());
    imageItem->setOpacity(factor);

    return imageItem;
}

QImage ScreenEdgeEffect::glowImage(ElectricBorder border, const QSize &size)
{
    if (border == ElectricLeft || border == ElectricRight || border == ElectricTop || border == ElectricBottom) {
        return edgeGlowImage(border, size);
    } else {
        return cornerGlowImage(border);
    }
}

QImage ScreenEdgeEffect::cornerGlowImage(ElectricBorder border)
{
    ensureGlowSvg();

    switch (border) {
    case ElectricTopLeft:
        return m_glow->pixmap(QStringLiteral("bottomright")).toImage();
    case ElectricTopRight:
        return m_glow->pixmap(QStringLiteral("bottomleft")).toImage();
    case ElectricBottomRight:
        return m_glow->pixmap(QStringLiteral("topleft")).toImage();
    case ElectricBottomLeft:
        return m_glow->pixmap(QStringLiteral("topright")).toImage();
    default:
        return QImage{};
    }
}

QImage ScreenEdgeEffect::edgeGlowImage(ElectricBorder border, const QSize &size)
{
    ensureGlowSvg();

    const bool stretchBorder = m_glow->hasElement(QStringLiteral("hint-stretch-borders"));

    QPoint pixmapPosition(0, 0);
    QPixmap l, r, c;
    switch (border) {
    case ElectricTop:
        l = m_glow->pixmap(QStringLiteral("bottomleft"));
        r = m_glow->pixmap(QStringLiteral("bottomright"));
        c = m_glow->pixmap(QStringLiteral("bottom"));
        break;
    case ElectricBottom:
        l = m_glow->pixmap(QStringLiteral("topleft"));
        r = m_glow->pixmap(QStringLiteral("topright"));
        c = m_glow->pixmap(QStringLiteral("top"));
        pixmapPosition = QPoint(0, size.height() - c.height());
        break;
    case ElectricLeft:
        l = m_glow->pixmap(QStringLiteral("topright"));
        r = m_glow->pixmap(QStringLiteral("bottomright"));
        c = m_glow->pixmap(QStringLiteral("right"));
        break;
    case ElectricRight:
        l = m_glow->pixmap(QStringLiteral("topleft"));
        r = m_glow->pixmap(QStringLiteral("bottomleft"));
        c = m_glow->pixmap(QStringLiteral("left"));
        pixmapPosition = QPoint(size.width() - c.width(), 0);
        break;
    default:
        return QImage{};
    }
    QPixmap image(size);
    image.fill(Qt::transparent);
    QPainter p;
    p.begin(&image);
    if (border == ElectricBottom || border == ElectricTop) {
        p.drawPixmap(pixmapPosition, l);
        const QRect cRect(l.width(), pixmapPosition.y(), size.width() - l.width() - r.width(), c.height());
        if (stretchBorder) {
            p.drawPixmap(cRect, c);
        } else {
            p.drawTiledPixmap(cRect, c);
        }
        p.drawPixmap(QPoint(size.width() - r.width(), pixmapPosition.y()), r);
    } else {
        p.drawPixmap(pixmapPosition, l);
        const QRect cRect(pixmapPosition.x(), l.height(), c.width(), size.height() - l.height() - r.height());
        if (stretchBorder) {
            p.drawPixmap(cRect, c);
        } else {
            p.drawTiledPixmap(cRect, c);
        }
        p.drawPixmap(QPoint(pixmapPosition.x(), size.height() - r.height()), r);
    }
    p.end();
    return image.toImage();
}

bool ScreenEdgeEffect::isActive() const
{
    return !m_glowItems.empty() && !effects->isScreenLocked();
}

} // namespace

#include "moc_screenedgeeffect.cpp"
