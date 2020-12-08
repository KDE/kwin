/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QPainter>

#include <KDecoration2/Decoration>
#include <KPluginFactory>


class FakeDecoWithShadows : public KDecoration2::Decoration
{
    Q_OBJECT

public:
    explicit FakeDecoWithShadows(QObject *parent = nullptr, const QVariantList &args = QVariantList())
        : Decoration(parent, args) {}
    ~FakeDecoWithShadows() override {}

    void paint(QPainter *painter, const QRect &repaintRegion) override {
        Q_UNUSED(painter)
        Q_UNUSED(repaintRegion)
    }

public Q_SLOTS:
    void init() override {
        const int shadowSize = 128;
        const int offsetTop = 64;
        const int offsetLeft = 48;
        const QRect shadowRect(0, 0, 4 * shadowSize + 1, 4 * shadowSize + 1);

        QImage shadowTexture(shadowRect.size(), QImage::Format_ARGB32_Premultiplied);
        shadowTexture.fill(Qt::transparent);

        const QMargins padding(
            shadowSize - offsetLeft,
            shadowSize - offsetTop,
            shadowSize + offsetLeft,
            shadowSize + offsetTop);

        auto decoShadow = QSharedPointer<KDecoration2::DecorationShadow>::create();
        decoShadow->setPadding(padding);
        decoShadow->setInnerShadowRect(QRect(shadowRect.center(), QSize(1, 1)));
        decoShadow->setShadow(shadowTexture);

        setShadow(decoShadow);
    }
};

K_PLUGIN_FACTORY_WITH_JSON(
    FakeDecoWithShadowsFactory,
    "fakedecoration_with_shadows.json",
    registerPlugin<FakeDecoWithShadows>();
)

#include "fakedecoration_with_shadows.moc"
