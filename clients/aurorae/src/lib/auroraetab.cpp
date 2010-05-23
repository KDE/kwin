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
#include <KDE/Plasma/FrameSvg>
#include <KDE/Plasma/PaintUtils>

namespace Aurorae
{

AuroraeTab::AuroraeTab(AuroraeTheme* theme, const QString& caption, int index)
    : QGraphicsWidget()
    , m_theme(theme)
    , m_caption(caption)
    , m_index(index)
    , m_dblClicked(false)
    , m_uid(-1)
    , m_dragAllowed(true)
    , m_clickInProgress(false)
{
    m_effect = new QGraphicsDropShadowEffect(this);
    if (m_theme->themeConfig().useTextShadow()) {
        setGraphicsEffect(m_effect);
    }
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

void AuroraeTab::setIndex(int index)
{
    m_index = index;
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
    const bool useTabs = (s->tabCount() > 1);
    const bool focused = s->isFocusedTab(m_index);
    const ThemeConfig &conf = m_theme->themeConfig();
    const DecorationPosition decoPos = s->isShade() ? DecorationTop : m_theme->decorationPosition();
    if (useTabs) {
        painter->save();
        Plasma::FrameSvg *decoration = m_theme->decoration();
        if (!decoration->hasElementPrefix("tab-active-focused") && m_index < s->tabCount()-1) {
            QColor color = active ? conf.activeTextColor(true, false) : conf.inactiveTextColor(true, false);
            painter->setPen(color);
            QPointF point1 = rect().topRight();
            QPointF point2 = rect().bottomRight();
            if (decoPos == DecorationLeft || decoPos == DecorationRight) {
                point1 = rect().topRight();
            point2 = rect().topLeft();
            }
            painter->drawLine(point1, point2);
        } else if (decoration->hasElementPrefix("tab-active-focused")) {
            QString element = "tab-active-";
            if (!active && decoration->hasElementPrefix("tab-inactive-focused")) {
                element = "tab-inactive-";
            }
            bool useFocused = true;
            if (!focused) {
                if (element.startsWith(QLatin1String("tab-active")) &&
                    decoration->hasElementPrefix("tab-active-unfocused")) {
                    useFocused = false;
                }
                if (element.startsWith(QLatin1String("tab-inactive")) &&
                    decoration->hasElementPrefix("tab-inactive-unfocused")) {
                    useFocused = false;
                }
            }
            element.append(useFocused ? "focused" : "unfocused");
            decoration->setElementPrefix(element);
            decoration->setEnabledBorders(Plasma::FrameSvg::AllBorders);
            decoration->resizeFrame(size());
            decoration->paintFrame(painter);
        }
        painter->restore();
    }
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
    qreal w = rect().width();
    qreal h = rect().height();
    if (decoPos == DecorationLeft || decoPos == DecorationRight) {
        h = rect().width();
        w = rect().height();
    }
    const QRect textRect = painter->fontMetrics().boundingRect(QRect(rect().x(), rect().y(), w, h),
            align | conf.verticalAlignment() | Qt::TextSingleLine,
            m_caption);
    if ((active && conf.haloActive()) ||
        (!active && conf.haloInactive())) {
        QRect haloRect = textRect;
        if (haloRect.width() > w) {
            haloRect.setWidth(w);
        }
        painter->save();
        if (decoPos == DecorationLeft) {
            painter->translate(rect().bottomLeft());
            painter->rotate(270);
        } else if (decoPos == DecorationRight) {
            painter->translate(rect().topRight());
            painter->rotate(90);
        }
        Plasma::PaintUtils::drawHalo(painter, haloRect);
        painter->restore();
    }
    QPixmap pix(w, h);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QColor color;
    if (active) {
        color = conf.activeTextColor(useTabs, focused);
        if (s->isAnimating()) {
            color = KColorUtils::mix(conf.inactiveTextColor(useTabs, focused), conf.activeTextColor(useTabs, focused), s->animationProgress());
        }
    } else {
        color = conf.inactiveTextColor(useTabs, focused);
        if (s->isAnimating()){
            color = KColorUtils::mix(conf.activeTextColor(useTabs, focused), conf.inactiveTextColor(useTabs, focused), s->animationProgress());
        }
    }
    p.setPen(color);
    p.drawText(pix.rect(), align | conf.verticalAlignment() | Qt::TextSingleLine, m_caption);
    if (textRect.width() > w) {
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
    if (decoPos == DecorationLeft) {
        painter->translate(rect().bottomLeft());
        painter->rotate(270);
    } else if (decoPos == DecorationRight) {
        painter->translate(rect().topRight());
        painter->rotate(90);
    }
    painter->drawPixmap(pix.rect(), pix);
    painter->restore();
}

void AuroraeTab::buttonSizesChanged()
{
    updateGeometry();
}

void AuroraeTab::setUniqueTabId(long int id)
{
    m_uid = id;
    if (m_clickInProgress) {
        m_dragAllowed = true;
    }
}

void AuroraeTab::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);
    event->accept();
    m_clickPos = event->pos();
    m_clickInProgress = true;
    m_dragAllowed = false;
    emit mouseButtonPress(event, m_index);
}

void AuroraeTab::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    m_dragAllowed = false;
    m_clickInProgress = false;
    if (m_dblClicked && event->button() == Qt::LeftButton) {
        // eat event
        m_dblClicked = false;
        return;
    }
    emit mouseButtonRelease(event, m_index);
}

void AuroraeTab::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    m_clickInProgress = false;
    if (event->button() == Qt::LeftButton) {
        m_dblClicked = true;
        emit mouseDblClicked();
    }
}

void AuroraeTab::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    AuroraeScene *s = static_cast<AuroraeScene*>(scene());
    if (s->tabCount() >= 1 && m_dragAllowed &&
        (event->pos() - m_clickPos).manhattanLength() >= KGlobalSettings::dndEventDelay()) {
        QDrag *drag = new QDrag(event->widget());
        QMimeData *data = new QMimeData;
        QString itemData = QString().setNum(m_uid) + '/' + QString().setNum(m_index);
        data->setData(m_theme->tabDragMimeType(), itemData.toAscii());
        drag->setMimeData(data);
        QPixmap pix(size().toSize());
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        s->render(&painter, pix.rect(), sceneBoundingRect());
        drag->setPixmap(pix);
        drag->exec();
        if (!drag->target() && s->tabCount() > 1) {
            emit tabRemoved(m_index);
        }
    }
}

} // namespace
