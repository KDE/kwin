/*
    Library for Aurorae window decoration themes.
    Copyright (C) 2009, 2010 Martin Gräßlin <kde@martin-graesslin.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include "auroraetab.h"
#include "auroraescene.h"
#include "auroraetheme.h"
#include "themeconfig.h"
#include <QtGui/QGraphicsDropShadowEffect>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <KDE/KColorUtils>
#include <KDE/KGlobalSettings>
#include <KDE/Plasma/PaintUtils>

namespace Aurorae
{

AuroraeTab::AuroraeTab(AuroraeTheme* theme, const QString& caption)
    : QGraphicsWidget()
    , m_theme(theme)
    , m_caption(caption)
{
    m_effect = new QGraphicsDropShadowEffect(this);
    if (m_theme->themeConfig().useTextShadow()) {
        setGraphicsEffect(m_effect);
    }
    setAcceptedMouseButtons(Qt::NoButton);
    connect(m_theme, SIGNAL(buttonSizesChanged()), SLOT(buttonSizesChanged()));
}

AuroraeTab::~AuroraeTab()
{
}

void AuroraeTab::activeChanged()
{
    const ThemeConfig &config = m_theme->themeConfig();
    if (!config.useTextShadow()) {
        return;
    }
    const bool active = static_cast<AuroraeScene*>(scene())->isActive();
    m_effect->setXOffset(config.textShadowOffsetX());
    m_effect->setYOffset(config.textShadowOffsetY());
    m_effect->setColor(active ? config.activeTextShadowColor() : config.inactiveTextShadowColor());
}

void AuroraeTab::setCaption(const QString& caption)
{
    if (m_caption == caption) {
        return;
    }
    m_caption = caption;
    updateGeometry();
    update();
}

void AuroraeTab::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    AuroraeScene *s = static_cast<AuroraeScene*>(scene());
    const bool active = s->isActive();
    const ThemeConfig &conf = m_theme->themeConfig();
    Qt::Alignment align = conf.alignment();
    if (align != Qt::AlignCenter && QApplication::layoutDirection() == Qt::RightToLeft) {
        // have to swap the alignment to be consistent with other kwin decos.
        // Decorations do not change in RTL mode
        if (align == Qt::AlignLeft) {
            align = Qt::AlignRight;
        } else {
            align = Qt::AlignLeft;
        }
    }
    const QRect textRect = painter->fontMetrics().boundingRect(rect().toRect(),
            align | conf.verticalAlignment() | Qt::TextSingleLine,
            m_caption);
    if ((active && conf.haloActive()) ||
        (!active && conf.haloInactive())) {
        QRect haloRect = textRect;
        if (haloRect.width() > rect().width()) {
            haloRect.setWidth(rect().width());
        }
        Plasma::PaintUtils::drawHalo(painter, haloRect);
    }
    QPixmap pix(rect().size().toSize());
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QColor color;
    if (active) {
        color = conf.activeTextColor();
        if (s->isAnimating()) {
            color = KColorUtils::mix(conf.inactiveTextColor(), conf.activeTextColor(), s->animationProgress());
        }
    } else {
        color = conf.inactiveTextColor();
        if (s->isAnimating()){
            color = KColorUtils::mix(conf.activeTextColor(), conf.inactiveTextColor(), s->animationProgress());
        }
    }
    p.setPen(color);
    p.drawText(pix.rect(), align | conf.verticalAlignment() | Qt::TextSingleLine, m_caption);
    if (textRect.width() > rect().width()) {
        // Fade out effect
        // based on fadeout of tasks applet in Plasma/desktop/applets/tasks/abstracttaskitem.cpp by
        // Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>
        // Copyright (C) 2008 by Alexis Ménard <darktears31@gmail.com>
        // Copyright (C) 2008 by Marco Martin <notmart@gmail.com>
        QLinearGradient alphaGradient(0, 0, 1, 0);
        alphaGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        if (QApplication::layoutDirection() == Qt::LeftToRight) {
            alphaGradient.setColorAt(0, QColor(0, 0, 0, 255));
            alphaGradient.setColorAt(1, QColor(0, 0, 0, 0));
        } else {
            alphaGradient.setColorAt(0, QColor(0, 0, 0, 0));
            alphaGradient.setColorAt(1, QColor(0, 0, 0, 255));
        }
        int fadeWidth = 30;
        int x = pix.width() - fadeWidth;
        QRect r = QStyle::visualRect(QApplication::layoutDirection(), pix.rect(),
                                    QRect(x, 0, fadeWidth, pix.height()));
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.fillRect(r, alphaGradient);
    }
    p.end();
    painter->drawPixmap(rect().toRect(), pix);
    painter->restore();
}

QSizeF AuroraeTab::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    QFont titleFont = KGlobalSettings::windowTitleFont();
    QFontMetricsF fm(titleFont);
    const qreal titleHeight = qMax((qreal)m_theme->themeConfig().titleHeight(),
                                   m_theme->themeConfig().buttonHeight()*m_theme->buttonSizeFactor() +
                                   m_theme->themeConfig().buttonMarginTop());
    switch (which) {
    case Qt::MinimumSize:
        return QSizeF(fm.boundingRect(m_caption.left(3)).width(), titleHeight);
    case Qt::PreferredSize:
        return QSizeF(fm.boundingRect(m_caption).width(), titleHeight);
    case Qt::MaximumSize:
        return QSizeF(scene()->sceneRect().width(), titleHeight);
    default:
        return QGraphicsWidget::sizeHint(which, constraint);
    }
    return QGraphicsWidget::sizeHint(which, constraint);
}

void AuroraeTab::buttonSizesChanged()
{
    updateGeometry();
}

} // namespace
