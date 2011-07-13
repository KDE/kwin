/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/


#include "desktopchangeosd.h"
#include <QTextStream>
#include "workspace.h"

#include <X11/extensions/shape.h>

#include <QDebug>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QHash>
#include <QGraphicsScene>
#include <QRect>
#include <KDE/Plasma/Theme>
#include <KDE/Plasma/PaintUtils>
#include <kiconloader.h>

namespace KWin
{

DesktopChangeOSD::DesktopChangeOSD(Workspace* ws)
    : QGraphicsView()
    , m_wspace(ws)
    , m_scene(0)
    , m_active(false)
    , m_show(false)
    , m_delayTime(0)
    , m_textOnly(false)
{
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    setFrameStyle(QFrame::NoFrame);
    viewport()->setAutoFillBackground(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAttribute(Qt::WA_TranslucentBackground);
    m_frame.setImagePath("dialogs/background");
    m_frame.setCacheAllRenderedFrames(true);
    m_frame.setEnabledBorders(Plasma::FrameSvg::AllBorders);

    m_item_frame.setImagePath("widgets/pager");
    m_item_frame.setCacheAllRenderedFrames(true);
    m_item_frame.setEnabledBorders(Plasma::FrameSvg::AllBorders);

    m_delayedHideTimer.setSingleShot(true);
    connect(&m_delayedHideTimer, SIGNAL(timeout()), this, SLOT(hide()));
    connect(ws, SIGNAL(currentDesktopChanged(int)), this, SLOT(desktopChanged(int)));
    connect(ws, SIGNAL(numberDesktopsChanged(int)), this, SLOT(numberDesktopsChanged()));
    connect(ws, SIGNAL(configChanged()), this, SLOT(reconfigure()));

    m_scene = new QGraphicsScene(0);
    setScene(m_scene);

    reconfigure();

    m_scene->addItem(new DesktopChangeText(m_wspace));
}

DesktopChangeOSD::~DesktopChangeOSD()
{
    delete m_scene;
}

void DesktopChangeOSD::reconfigure()
{
    KSharedConfigPtr c(KGlobal::config());
    const KConfigGroup cg = c->group("PopupInfo");
    m_show = cg.readEntry("ShowPopup", false);
    m_delayTime = cg.readEntry("PopupHideDelay", 1000);
    m_textOnly = cg.readEntry("TextOnly", false);
    numberDesktopsChanged();
}

void DesktopChangeOSD::desktopChanged(int old)
{
    // Not for the very first time, only if something changed and there are more than 1 desktops
    if (old == 0 || old == m_wspace->currentDesktop() || m_wspace->numberOfDesktops() <= 1)
        return;
    if (!m_show)
        return;
    // we have to stop in case the old desktop does not exist anymore
    if (old > m_wspace->numberOfDesktops())
        return;
    // calculate where icons have to be shown
    QPoint diff = m_wspace->desktopGridCoords(m_wspace->currentDesktop()) - m_wspace->desktopGridCoords(old);
    QHash< int, DesktopChangeItem::Arrow > hash = QHash< int, DesktopChangeItem::Arrow>();
    int desktop = old;
    int target = m_wspace->currentDesktop();
    int x = diff.x();
    int y = diff.y();
    if (y >= 0) {
        // first go in x direction, then in y
        while (desktop != target) {
            if (x != 0) {
                if (x < 0) {
                    x++;
                    hash.insert(desktop, DesktopChangeItem::LEFT);
                    desktop = m_wspace->desktopToLeft(desktop);
                } else {
                    x--;
                    hash.insert(desktop, DesktopChangeItem::RIGHT);
                    desktop = m_wspace->desktopToRight(desktop);
                }
                continue;
            }
            y--;
            hash.insert(desktop, DesktopChangeItem::DOWN);
            desktop = m_wspace->desktopBelow(desktop);
        }
    } else {
        // first go in y direction, then in x
        while (target != desktop) {
            if (y != 0) {
                // only go upward
                y++;
                hash.insert(desktop, DesktopChangeItem::UP);
                desktop = m_wspace->desktopAbove(desktop);
                continue;
            }
            if (x != 0) {
                if (x < 0) {
                    x++;
                    hash.insert(desktop, DesktopChangeItem::LEFT);
                    desktop = m_wspace->desktopToLeft(desktop);
                } else {
                    x--;
                    hash.insert(desktop, DesktopChangeItem::RIGHT);
                    desktop = m_wspace->desktopToRight(desktop);
                }
            }
        }
    }
    // now we know which desktop has to show an arrow -> set the arrow for each desktop
    int numberOfArrows = qAbs(diff.x()) + qAbs(diff.y());
    foreach (QGraphicsItem * it, m_scene->items()) {
        DesktopChangeItem* item = qgraphicsitem_cast< DesktopChangeItem* >(it);
        if (item) {
            if (hash.contains(item->desktop())) {
                QPoint distance = m_wspace->desktopGridCoords(m_wspace->currentDesktop())
                                  - m_wspace->desktopGridCoords(item->desktop());
                int desktopDistance = numberOfArrows - (qAbs(distance.x()) + qAbs(distance.y()));
                int start = m_delayTime / numberOfArrows * desktopDistance - m_delayTime * 0.15f;
                int stop = m_delayTime / numberOfArrows * (desktopDistance + 1) + m_delayTime * 0.15f;
                start = qMax(start, 0);
                item->setArrow(hash[ item->desktop()], start, stop);
            } else {
                item->setArrow(DesktopChangeItem::NONE, 0, 0);
            }
            if (old != m_wspace->currentDesktop()) {
                if (item->desktop() == m_wspace->currentDesktop())
                    item->startDesktopHighLightAnimation(m_delayTime * 0.33);
                if (m_active && item->desktop() == old)
                    item->stopDesktopHighLightAnimation();
            }
        }
    }
    if (m_active) {
        // for text only we need to resize
        if (m_textOnly)
            resize();
        // already active - just update and reset timer
        update();
    } else {
        m_active = true;
        resize();
        show();
        raise();
    }
    // Set a zero inputmask, effectively making clicks go "through" the popup
    // For those who impatiently wait to click on a dialog behind the it
    XShapeCombineRectangles(display(), winId(), ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    m_delayedHideTimer.start(m_delayTime);
}

void DesktopChangeOSD::hideEvent(QHideEvent*)
{
    m_delayedHideTimer.stop();
    m_active = false;
}

void DesktopChangeOSD::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    qreal left, top, right, bottom;
    m_frame.getMargins(left, top, right, bottom);
    m_frame.paintFrame(painter, rect.adjusted(-left, -top, right, bottom));
    painter->restore();
}

void DesktopChangeOSD::numberDesktopsChanged()
{
    foreach (QGraphicsItem * it, m_scene->items()) {
        DesktopChangeItem* item = qgraphicsitem_cast<DesktopChangeItem*>(it);
        if (item) {
            m_scene->removeItem(item);
        }
    }

    if (!m_textOnly) {
        for (int i = 1; i <= m_wspace->numberOfDesktops(); i++) {
            DesktopChangeItem* item = new DesktopChangeItem(m_wspace, this, i);
            m_scene->addItem(item);
        }
    }
}

void DesktopChangeOSD::resize()
{
    QRect screenRect = m_wspace->clientArea(ScreenArea, m_wspace->activeScreen(), m_wspace->currentDesktop());
    QRect fullRect = m_wspace->clientArea(FullArea, m_wspace->activeScreen(), m_wspace->currentDesktop());
    qreal left, top, right, bottom;
    m_frame.getMargins(left, top, right, bottom);

    QSize desktopGridSize = m_wspace->desktopGridSize();
    float itemWidth = fullRect.width() * 0.1f;
    float itemHeight = fullRect.height() * 0.1f;
    // 2 px distance between each desktop + each desktop a width of 5 % of full screen + borders
    float width = (desktopGridSize.width() - 1) * 2 + desktopGridSize.width() * itemWidth + left + right;
    float height = (desktopGridSize.height() - 1) * 2 + top + bottom;

    // bound width between ten and 33 percent of active screen
    float tempWidth = qBound(screenRect.width() * 0.25f, width, screenRect.width() * 0.5f);
    if (tempWidth != width) {
        // have to adjust the height
        width = tempWidth;
        itemWidth = (width - (desktopGridSize.width() - 1) * 2 - left - right) / desktopGridSize.width();
        itemHeight = itemWidth * (float)((float)fullRect.height() / (float)fullRect.width());
    }
    height += itemHeight * desktopGridSize.height();
    height += fontMetrics().height() + 4;

    // we do not increase height, but it's bound to a third of screen height
    float tempHeight = qMin(height, screenRect.height() * 0.5f);
    float itemOffset = 0.0f;
    if (tempHeight != height) {
        // have to adjust item width
        height = tempHeight;
        itemHeight = (height - (fontMetrics().height() + 4) - top - bottom - (desktopGridSize.height() - 1) * 2) /
                     desktopGridSize.height();
        itemOffset = itemWidth;
        itemWidth = itemHeight * (float)((float)fullRect.width() / (float)fullRect.height());
        itemOffset -= itemWidth;
        itemOffset *= (float)desktopGridSize.width() * 0.5f;
    }

    // set size to the desktop name if the "pager" is not shown
    if (m_textOnly) {
        height = fontMetrics().height() + 4 + top + bottom;
        width = fontMetrics().boundingRect(m_wspace->desktopName(m_wspace->currentDesktop())).width() +
                4 + left + right;
    }

    QRect rect = QRect(screenRect.x() + (screenRect.width() - width) / 2,
                       screenRect.y() + (screenRect.height() - height) / 2,
                       width,
                       height);
    setGeometry(rect);
    m_scene->setSceneRect(0, 0, width, height);
    m_frame.resizeFrame(QSize(width, height));

    if (Plasma::Theme::defaultTheme()->windowTranslucencyEnabled()) {
        // blur background
        Plasma::WindowEffects::enableBlurBehind(winId(), true, m_frame.mask());
        Plasma::WindowEffects::overrideShadow(winId(), true);
    } else {
        // do not trim to mask with compositing enabled, otherwise shadows are cropped
        setMask(m_frame.mask());
    }

    // resize item frame
    m_item_frame.setElementPrefix("normal");
    m_item_frame.resizeFrame(QSize(itemWidth, itemHeight));
    m_item_frame.setElementPrefix("hover");
    m_item_frame.resizeFrame(QSize(itemWidth, itemHeight));

    // reset the items
    foreach (QGraphicsItem * it, m_scene->items()) {
        DesktopChangeItem* item = qgraphicsitem_cast<DesktopChangeItem*>(it);
        if (item) {
            item->setWidth(itemWidth);
            item->setHeight(itemHeight);
            QPoint coords = m_wspace->desktopGridCoords(item->desktop());
            item->setPos(left + itemOffset + coords.x()*(itemWidth + 2),
                         top + fontMetrics().height() + 4 + coords.y()*(itemHeight + 4));
        }
        DesktopChangeText* text = qgraphicsitem_cast<DesktopChangeText*>(it);
        if (text) {
            text->setPos(left, top);
            text->setWidth(width - left - right);
            if (m_textOnly)
                text->setHeight(fontMetrics().height() + 4);
            else
                text->setHeight(fontMetrics().height());
        }
    }
}

//*******************************
// DesktopChangeText
//*******************************
DesktopChangeText::DesktopChangeText(Workspace* ws)
    : QGraphicsItem()
    , m_wspace(ws)
    , m_width(0.0f)
    , m_height(0.0f)
{
}

DesktopChangeText::~DesktopChangeText()
{
}

QRectF DesktopChangeText::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void DesktopChangeText::paint(QPainter* painter, const QStyleOptionGraphicsItem* , QWidget*)
{
    painter->setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    painter->drawText(boundingRect(), Qt::AlignCenter | Qt:: AlignVCenter,
                      m_wspace->desktopName(m_wspace->currentDesktop()));
}

//*******************************
// DesktopChangeItem
//*******************************
DesktopChangeItem::DesktopChangeItem(Workspace* ws, DesktopChangeOSD* parent, int desktop)
    : QGraphicsItem()
    , m_wspace(ws)
    , m_parent(parent)
    , m_desktop(desktop)
    , m_width(0.0f)
    , m_height(0.0f)
    , m_arrow(NONE)
    , m_arrowShown(false)
    , m_fadeInArrow(false)
    , m_fadeInHighLight(false)
    , m_arrowValue(0.0)
    , m_highLightValue(0.0)
{
    m_delayed_show_arrow_timer.setSingleShot(true);
    m_delayed_hide_arrow_timer.setSingleShot(true);
    connect(&m_delayed_show_arrow_timer, SIGNAL(timeout()), this, SLOT(showArrow()));
    connect(&m_delayed_hide_arrow_timer, SIGNAL(timeout()), this, SLOT(hideArrow()));
}

DesktopChangeItem::~DesktopChangeItem()
{
}

QRectF DesktopChangeItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void DesktopChangeItem::setArrow(Arrow arrow, int start_delay, int hide_delay)
{
    // stop timers
    m_delayed_show_arrow_timer.stop();
    m_delayed_hide_arrow_timer.stop();

    QPropertyAnimation *arrowAnimation = m_arrowAnimation.data();
    if (arrowAnimation) {
        arrowAnimation->stop();
        m_arrowAnimation.clear();
    }

    m_arrowShown = false;
    m_arrow = arrow;
    if (m_arrow != NONE) {
        m_delayed_show_arrow_timer.start(start_delay);
        m_delayed_hide_arrow_timer.start(hide_delay);
    }
}

qreal DesktopChangeItem::arrowValue() const
{
    qCritical() << __func__ << m_arrowValue;
    return m_arrowValue;
}

qreal DesktopChangeItem::highLightValue() const
{
    return m_highLightValue;
}

void DesktopChangeItem::setArrowValue(qreal value)
{
    m_arrowValue = value;

    update();
}

void DesktopChangeItem::setHighLightValue(qreal value)
{
    m_highLightValue = value;

    update();
}

void DesktopChangeItem::showArrow()
{
    m_arrowShown = true;

    QPropertyAnimation *arrowAnimation = m_arrowAnimation.data();
    if (!arrowAnimation) {
        arrowAnimation = new QPropertyAnimation(this, "arrowValue");
        arrowAnimation->setDuration(m_parent->getDelayTime() * 0.15f);
        arrowAnimation->setStartValue(0.0);
        arrowAnimation->setEndValue(1.0);

        m_arrowAnimation = arrowAnimation;
    }

    m_fadeInArrow = true;

    arrowAnimation->setEasingCurve(QEasingCurve::InQuad);
    arrowAnimation->setDirection(QAbstractAnimation::Forward);
    arrowAnimation->start();
}

void DesktopChangeItem::hideArrow()
{
    m_fadeInArrow = false;

    QPropertyAnimation *arrowAnimation = m_arrowAnimation.data();
    if (arrowAnimation) {
        arrowAnimation->setEasingCurve(QEasingCurve::OutQuad);
        arrowAnimation->setDirection(QAbstractAnimation::Backward);
        arrowAnimation->start(QAbstractAnimation::DeleteWhenStopped);

        connect(arrowAnimation, SIGNAL(finished()), this, SLOT(arrowAnimationFinished()));
    }
}

void DesktopChangeItem::startDesktopHighLightAnimation(int time)
{
    QPropertyAnimation *highLightAnimation = m_highLightAnimation.data();
    if (!highLightAnimation) {
        highLightAnimation = new QPropertyAnimation(this, "highLightValue");
        highLightAnimation->setDuration(time);
        highLightAnimation->setStartValue(0.0);
        highLightAnimation->setEndValue(1.0);

        m_highLightAnimation = highLightAnimation;
    }

    m_fadeInHighLight = true;

    highLightAnimation->setEasingCurve(QEasingCurve::InQuad);
    highLightAnimation->setDirection(QAbstractAnimation::Forward);
    highLightAnimation->start();
}

void DesktopChangeItem::stopDesktopHighLightAnimation()
{
    m_fadeInHighLight = false;

    QPropertyAnimation *highLightAnimation = m_highLightAnimation.data();
    if (highLightAnimation) {
        highLightAnimation->setEasingCurve(QEasingCurve::OutQuad);
        highLightAnimation->setDirection(QAbstractAnimation::Backward);
        highLightAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void DesktopChangeItem::arrowAnimationFinished()
{
    if (!m_fadeInArrow)
        m_arrowShown = false;
}

void DesktopChangeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* , QWidget*)
{
    if (m_wspace->currentDesktop() == m_desktop || (!m_highLightAnimation.isNull() &&
            m_highLightAnimation.data()->state() == QAbstractAnimation::Running)) {
        qreal left, top, right, bottom;
        m_parent->itemFrame()->getMargins(left, top, right, bottom);
        if (!m_highLightAnimation.isNull() &&
                m_highLightAnimation.data()->state() == QAbstractAnimation::Running) {
            // there is an animation - so we use transition from normal to active or vice versa
            if (m_fadeInHighLight) {
                m_parent->itemFrame()->setElementPrefix("normal");
                QPixmap normal = m_parent->itemFrame()->framePixmap();
                m_parent->itemFrame()->setElementPrefix("hover");
                QPixmap result = Plasma::PaintUtils::transition(normal,
                                 m_parent->itemFrame()->framePixmap(), m_highLightValue);
                painter->drawPixmap(boundingRect().toRect(), result);
            } else {
                m_parent->itemFrame()->setElementPrefix("hover");
                QPixmap normal = m_parent->itemFrame()->framePixmap();
                m_parent->itemFrame()->setElementPrefix("normal");
                QPixmap result = Plasma::PaintUtils::transition(normal,
                                 m_parent->itemFrame()->framePixmap(), 1.0 - m_highLightValue);
                painter->drawPixmap(boundingRect().toRect(), result);
            }
        } else {
            // no animation - just render the active frame
            m_parent->itemFrame()->setElementPrefix("hover");
            m_parent->itemFrame()->paintFrame(painter, boundingRect());
        }
        QColor rectColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
        rectColor.setAlphaF(0.6 * m_highLightValue);
        QBrush rectBrush = QBrush(rectColor);
        painter->fillRect(boundingRect().adjusted(left, top, -right, -bottom), rectBrush);
    } else {
        m_parent->itemFrame()->setElementPrefix("normal");
        m_parent->itemFrame()->paintFrame(painter, boundingRect());
    }

    if (!m_arrowShown)
        return;
    // paint the arrow
    QPixmap icon;
    int iconWidth = 32;
    qreal maxsize = qMin(boundingRect().width(), boundingRect().height());
    if (maxsize > 128.0)
        iconWidth = 128;
    else if (maxsize > 64.0)
        iconWidth = 64.0;
    else if (maxsize > 32.0)
        iconWidth = 32.0;
    else
        iconWidth = 16.0;
    QRect iconRect = QRect(boundingRect().x() + boundingRect().width() / 2 - iconWidth / 2,
                           boundingRect().y() + boundingRect().height() / 2 - iconWidth / 2,
                           iconWidth, iconWidth);
    switch(m_arrow) {
    case UP:
        icon = KIconLoader::global()->loadIcon("go-up", KIconLoader::Desktop, iconWidth);
        break;
    case DOWN:
        icon = KIconLoader::global()->loadIcon("go-down", KIconLoader::Desktop, iconWidth);
        break;
    case LEFT:
        icon = KIconLoader::global()->loadIcon("go-previous", KIconLoader::Desktop, iconWidth);
        break;
    case RIGHT:
        icon = KIconLoader::global()->loadIcon("go-next", KIconLoader::Desktop, iconWidth);
        break;
    default:
        break;
    }
    if (m_arrow != NONE) {
        if (!m_arrowAnimation.isNull() &&
                m_arrowAnimation.data()->state() == QAbstractAnimation::Running &&
                !qFuzzyCompare(m_arrowValue, qreal(1.0))) {
            QPixmap temp(icon.size());
            temp.fill(Qt::transparent);

            QPainter p(&temp);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.drawPixmap(0, 0, icon);
            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            p.fillRect(temp.rect(), QColor(0, 0, 0, 255 * m_arrowValue));
            p.end();

            icon = temp;
        }
        painter->drawPixmap(iconRect, icon);
    }
}
}

#include "desktopchangeosd.moc"
