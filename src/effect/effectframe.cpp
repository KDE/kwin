/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/effectframe.h"
#include "effect/effecthandler.h"

#include <QQuickItem>
#include <QStandardPaths>

namespace KWin
{

EffectFrameQuickScene::EffectFrameQuickScene(EffectFrameStyle style, bool staticSize, QPoint position, Qt::Alignment alignment)
    : m_style(style)
    , m_static(staticSize)
    , m_point(position)
    , m_alignment(alignment)
{

    QString name;
    switch (style) {
    case EffectFrameNone:
        name = QStringLiteral("none");
        break;
    case EffectFrameUnstyled:
        name = QStringLiteral("unstyled");
        break;
    case EffectFrameStyled:
        name = QStringLiteral("styled");
        break;
    }

    const QString defaultPath = KWIN_DATADIR + QStringLiteral("/frames/plasma/frame_%1.qml").arg(name);
    // TODO read from kwinApp()->config() "QmlPath" like Outline/OnScreenNotification
    // *if* someone really needs this to be configurable.
    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, defaultPath);

    setSource(QUrl::fromLocalFile(path), QVariantMap{{QStringLiteral("effectFrame"), QVariant::fromValue(this)}});

    if (rootItem()) {
        connect(rootItem(), &QQuickItem::implicitWidthChanged, this, &EffectFrameQuickScene::reposition);
        connect(rootItem(), &QQuickItem::implicitHeightChanged, this, &EffectFrameQuickScene::reposition);
    }
}

EffectFrameQuickScene::~EffectFrameQuickScene() = default;

EffectFrameStyle EffectFrameQuickScene::style() const
{
    return m_style;
}

bool EffectFrameQuickScene::isStatic() const
{
    return m_static;
}

QFont EffectFrameQuickScene::font() const
{
    return m_font;
}

void EffectFrameQuickScene::setFont(const QFont &font)
{
    if (m_font == font) {
        return;
    }

    m_font = font;
    Q_EMIT fontChanged(font);
    reposition();
}

QIcon EffectFrameQuickScene::icon() const
{
    return m_icon;
}

void EffectFrameQuickScene::setIcon(const QIcon &icon)
{
    m_icon = icon;
    Q_EMIT iconChanged(icon);
    reposition();
}

QSize EffectFrameQuickScene::iconSize() const
{
    return m_iconSize;
}

void EffectFrameQuickScene::setIconSize(const QSize &iconSize)
{
    if (m_iconSize == iconSize) {
        return;
    }

    m_iconSize = iconSize;
    Q_EMIT iconSizeChanged(iconSize);
    reposition();
}

QString EffectFrameQuickScene::text() const
{
    return m_text;
}

void EffectFrameQuickScene::setText(const QString &text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    Q_EMIT textChanged(text);
    reposition();
}

qreal EffectFrameQuickScene::frameOpacity() const
{
    return m_frameOpacity;
}

void EffectFrameQuickScene::setFrameOpacity(qreal frameOpacity)
{
    if (m_frameOpacity != frameOpacity) {
        m_frameOpacity = frameOpacity;
        Q_EMIT frameOpacityChanged(frameOpacity);
    }
}

bool EffectFrameQuickScene::crossFadeEnabled() const
{
    return m_crossFadeEnabled;
}

void EffectFrameQuickScene::setCrossFadeEnabled(bool enabled)
{
    if (m_crossFadeEnabled != enabled) {
        m_crossFadeEnabled = enabled;
        Q_EMIT crossFadeEnabledChanged(enabled);
    }
}

qreal EffectFrameQuickScene::crossFadeProgress() const
{
    return m_crossFadeProgress;
}

void EffectFrameQuickScene::setCrossFadeProgress(qreal progress)
{
    if (m_crossFadeProgress != progress) {
        m_crossFadeProgress = progress;
        Q_EMIT crossFadeProgressChanged(progress);
    }
}

Qt::Alignment EffectFrameQuickScene::alignment() const
{
    return m_alignment;
}

void EffectFrameQuickScene::setAlignment(Qt::Alignment alignment)
{
    if (m_alignment == alignment) {
        return;
    }

    m_alignment = alignment;
    reposition();
}

QPoint EffectFrameQuickScene::position() const
{
    return m_point;
}

void EffectFrameQuickScene::setPosition(const QPoint &point)
{
    if (m_point == point) {
        return;
    }

    m_point = point;
    reposition();
}

void EffectFrameQuickScene::reposition()
{
    if (!rootItem() || m_point.x() < 0 || m_point.y() < 0) {
        return;
    }

    QSizeF size;
    if (m_static) {
        size = rootItem()->size();
    } else {
        size = QSizeF(rootItem()->implicitWidth(), rootItem()->implicitHeight());
    }

    QRect geometry(QPoint(), size.toSize());

    if (m_alignment & Qt::AlignLeft)
        geometry.moveLeft(m_point.x());
    else if (m_alignment & Qt::AlignRight)
        geometry.moveLeft(m_point.x() - geometry.width());
    else
        geometry.moveLeft(m_point.x() - geometry.width() / 2);
    if (m_alignment & Qt::AlignTop)
        geometry.moveTop(m_point.y());
    else if (m_alignment & Qt::AlignBottom)
        geometry.moveTop(m_point.y() - geometry.height());
    else
        geometry.moveTop(m_point.y() - geometry.height() / 2);

    if (geometry == this->geometry()) {
        return;
    }

    setGeometry(geometry);
}

EffectFrame::EffectFrame(EffectFrameStyle style, bool staticSize, QPoint position, Qt::Alignment alignment)
    : m_view(new EffectFrameQuickScene(style, staticSize, position, alignment))
{
    connect(m_view, &OffscreenQuickScene::repaintNeeded, this, [this] {
        effects->addRepaint(geometry());
    });
    connect(m_view, &OffscreenQuickScene::geometryChanged, this, [](const QRect &oldGeometry, const QRect &newGeometry) {
        effects->addRepaint(oldGeometry);
        effects->addRepaint(newGeometry);
    });
}

EffectFrame::~EffectFrame()
{
    // Effects often destroy their cached TextFrames in pre/postPaintScreen.
    // Destroying an OffscreenQuickView changes GL context, which we
    // must not do during effect rendering.
    // Delay destruction of the view until after the rendering.
    m_view->deleteLater();
}

Qt::Alignment EffectFrame::alignment() const
{
    return m_view->alignment();
}

void EffectFrame::setAlignment(Qt::Alignment alignment)
{
    m_view->setAlignment(alignment);
}

QFont EffectFrame::font() const
{
    return m_view->font();
}

void EffectFrame::setFont(const QFont &font)
{
    m_view->setFont(font);
}

void EffectFrame::free()
{
    m_view->hide();
}

QRect EffectFrame::geometry() const
{
    return m_view->geometry();
}

void EffectFrame::setGeometry(const QRect &geometry, bool force)
{
    m_view->setGeometry(geometry);
}

QIcon EffectFrame::icon() const
{
    return m_view->icon();
}

void EffectFrame::setIcon(const QIcon &icon)
{
    m_view->setIcon(icon);

    if (m_view->iconSize().isEmpty() && !icon.availableSizes().isEmpty()) { // Set a size if we don't already have one
        setIconSize(icon.availableSizes().constFirst());
    }
}

QSize EffectFrame::iconSize() const
{
    return m_view->iconSize();
}

void EffectFrame::setIconSize(const QSize &size)
{
    m_view->setIconSize(size);
}

void EffectFrame::setPosition(const QPoint &point)
{
    m_view->setPosition(point);
}

void EffectFrame::render(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region, double opacity, double frameOpacity)
{
    if (!m_view->rootItem()) {
        return;
    }

    m_view->show();

    m_view->setOpacity(opacity);
    m_view->setFrameOpacity(frameOpacity);

    effects->renderOffscreenQuickView(renderTarget, viewport, m_view);
}

QString EffectFrame::text() const
{
    return m_view->text();
}

void EffectFrame::setText(const QString &text)
{
    m_view->setText(text);
}

EffectFrameStyle EffectFrame::style() const
{
    return m_view->style();
}

bool EffectFrame::isCrossFade() const
{
    return m_view->crossFadeEnabled();
}

void EffectFrame::enableCrossFade(bool enable)
{
    m_view->setCrossFadeEnabled(enable);
}

qreal EffectFrame::crossFadeProgress() const
{
    return m_view->crossFadeProgress();
}

void EffectFrame::setCrossFadeProgress(qreal progress)
{
    m_view->setCrossFadeProgress(progress);
}

} // namespace KWin

#include "moc_effectframe.cpp"
