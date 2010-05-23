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

#include "auroraescene.h"
#include "auroraebutton.h"
#include "auroraetab.h"
#include "auroraetheme.h"
#include "themeconfig.h"
// Qt
#include <QtCore/QPropertyAnimation>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsView>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QWidget>
// KDE
#include <KDE/Plasma/FrameSvg>
#include <KDE/Plasma/PaintUtils>

namespace Aurorae {

AuroraeScene::AuroraeScene(Aurorae::AuroraeTheme* theme, const QString& leftButtons,
                           const QString& rightButtons, bool contextHelp, QObject* parent)
    : QGraphicsScene(parent)
    , m_theme(theme)
    , m_leftButtons(0)
    , m_rightButtons(0)
    , m_title(0)
    , m_active(false)
    , m_animationProgress(0.0)
    , m_animation(new QPropertyAnimation(this, "animation", this))
    , m_maximizeMode(KDecorationDefines::MaximizeRestore)
    , m_allDesktops(false)
    , m_shade(false)
    , m_keepAbove(false)
    , m_keepBelow(false)
    , m_leftButtonOrder(leftButtons)
    , m_rightButtonOrder(rightButtons)
    , m_dblClicked(false)
    , m_contextHelp(contextHelp)
    , m_tabCount(0)
{
    init();
    connect(m_theme, SIGNAL(themeChanged()), SLOT(resetTheme()));
    connect(m_theme, SIGNAL(showTooltipsChanged(bool)), SLOT(showTooltipsChanged(bool)));
}

AuroraeScene::~AuroraeScene()
{
}

void AuroraeScene::init()
{
    if (!m_theme->isValid()) {
        return;
    }
    m_tabCount = 0;
    Qt::Orientation orientation = Qt::Horizontal;
    switch ((DecorationPosition)m_theme->themeConfig().decorationPosition()) {
    case DecorationLeft: // fall through
    case DecorationRight:
        orientation = Qt::Vertical;
        break;
    case DecorationTop: // fall through
    case DecorationBottom: // fall through
    default: // fall through
        orientation = Qt::Horizontal;
        break;
    }
    // left buttons
    QGraphicsLinearLayout *leftButtonsLayout = new QGraphicsLinearLayout;
    leftButtonsLayout->setSpacing(m_theme->themeConfig().buttonSpacing());
    leftButtonsLayout->setContentsMargins(0, 0, 0, 0);
    leftButtonsLayout->setOrientation(orientation);
    initButtons(leftButtonsLayout, buttonsToDirection(m_leftButtonOrder));

    m_leftButtons = new AuroraeButtonGroup(m_theme, AuroraeButtonGroup::LeftGroup);
    m_leftButtons->setLayout(leftButtonsLayout);
    addItem(m_leftButtons);

    // right buttons
    QGraphicsLinearLayout *rightButtonsLayout = new QGraphicsLinearLayout;
    rightButtonsLayout->setSpacing(m_theme->themeConfig().buttonSpacing());
    rightButtonsLayout->setContentsMargins(0, 0, 0, 0);
    rightButtonsLayout->setOrientation(orientation);
    initButtons(rightButtonsLayout, buttonsToDirection(m_rightButtonOrder));

    m_rightButtons = new AuroraeButtonGroup(m_theme, AuroraeButtonGroup::RightGroup);
    m_rightButtons->setLayout(rightButtonsLayout);
    addItem(m_rightButtons);

    // title area
    QGraphicsLinearLayout *titleLayout = new QGraphicsLinearLayout;
    titleLayout->setSpacing(0);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setOrientation(orientation);
    m_title = new QGraphicsWidget;
    m_title->setLayout(titleLayout);
    addItem(m_title);

    setActive(m_active, false);
    updateLayout();
    // reset the icon
    setIcon(m_iconPixmap);
    update(sceneRect());
}

void AuroraeScene::resetTheme()
{
    clear();
    init();
}

void AuroraeScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    if (!m_theme->isValid()) {
        return;
    }
    painter->setClipRect(rect);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    bool maximized = m_maximizeMode == KDecorationDefines::MaximizeFull; // TODO: check option
    const ThemeConfig &conf = m_theme->themeConfig();

    Plasma::FrameSvg *frame = m_theme->decoration();
    frame->setElementPrefix("decoration");
    if (!isActive() && frame->hasElementPrefix("decoration-inactive")) {
        frame->setElementPrefix("decoration-inactive");
    }
    if (!m_theme->isCompositingActive() && frame->hasElementPrefix("decoration-opaque")) {
        frame->setElementPrefix("decoration-opaque");
        if (!isActive() && frame->hasElementPrefix("decoration-opaque-inactive")) {
            frame->setElementPrefix("decoration-opaque-inactive");
        }
    }
    if (maximized) {
        if (frame->hasElementPrefix("decoration-maximized")) {
            frame->setElementPrefix("decoration-maximized");
        }
        if (!isActive() && frame->hasElementPrefix("decoration-maximized-inactive")) {
            frame->setElementPrefix("decoration-maximized-inactive");
        }
        if (!m_theme->isCompositingActive() && frame->hasElementPrefix("decoration-maximized-opaque")) {
            frame->setElementPrefix("decoration-maximized-opaque");
            if (!isActive() && frame->hasElementPrefix("decoration-maximized-opaque-inactive")) {
                frame->setElementPrefix("decoration-maximized-opaque-inactive");
            }
        }
    }

    // restrict painting on the decoration - no need to paint behind the window
    /*int left, right, top, bottom;
    decoration()->borders(left, right, top, bottom);
    if (!compositingActive() || (compositingActive() && !transparentRect().isNull())) {
        // only clip when compositing is not active and we don't extend into the client
        painter.setClipping(true);
        painter.setClipRect(0, 0,
                            left + conf.paddingLeft(),
                            height() + conf.paddingTop() + conf.paddingBottom(),
                            Qt::ReplaceClip);
        painter.setClipRect(0, 0,
                            width() + conf.paddingLeft() + conf.paddingRight(),
                            top + conf.paddingTop(),
                            Qt::UniteClip);
        painter.setClipRect(width() - right + conf.paddingLeft(), 0,
                            right + conf.paddingRight(),
                            height() + conf.paddingTop() + conf.paddingBottom(),
                            Qt::UniteClip);
        painter.setClipRect(0, height() - bottom + conf.paddingTop(),
                            width() + conf.paddingLeft() + conf.paddingRight(),
                            bottom + conf.paddingBottom(),
                            Qt::UniteClip);
    }*/

    // top
    if (maximized) {
        frame->setEnabledBorders(Plasma::FrameSvg::NoBorder);
    } else {
        frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    }
    QRectF r = sceneRect();
    const qreal titleHeight = qMax((qreal)conf.titleHeight(),
                                   conf.buttonHeight()*m_theme->buttonSizeFactor() + conf.buttonMarginTop());
    if (maximized) {
        r = QRectF(conf.paddingLeft(), conf.paddingTop(),
                      sceneRect().width() - conf.paddingRight() - conf.paddingLeft(),
                      sceneRect().height() - conf.paddingBottom() - conf.paddingTop());
        if (true/*transparentRect().isNull()*/) {
            switch ((DecorationPosition)conf.decorationPosition()) {
            case DecorationTop:
                r = QRectF(conf.paddingLeft(), conf.paddingTop(),
                          sceneRect().width() - conf.paddingRight() - conf.paddingLeft(),
                          conf.titleEdgeTopMaximized() + titleHeight + conf.titleEdgeBottomMaximized());
                break;
            case DecorationBottom: {
                const int h = conf.titleEdgeTopMaximized() + titleHeight + conf.titleEdgeBottomMaximized();
                r = QRectF(conf.paddingLeft(),
                           height() - conf.paddingBottom() - h,
                           sceneRect().width() - conf.paddingRight() - conf.paddingLeft(),
                           h);
                break;
            }
            case DecorationLeft:
                r = QRectF(conf.paddingLeft(), conf.paddingTop(),
                           conf.titleEdgeLeftMaximized() + titleHeight + conf.titleEdgeRightMaximized(),
                           height() - conf.paddingBottom() - conf.paddingTop());
                break;
            case DecorationRight: {
                const int w = conf.titleEdgeLeftMaximized() + titleHeight + conf.titleEdgeRightMaximized();
                r = QRectF(width() - conf.paddingRight() - w, conf.paddingTop(),
                           w, height() - conf.paddingBottom() - conf.paddingTop());
                break;
            }
            }
        }
    }
    QRectF sourceRect = QRectF(QPointF(0, 0), r.size());
    if (!m_theme->isCompositingActive()) {
        if (frame->hasElementPrefix("decoration-opaque")) {
            r = QRectF(conf.paddingLeft(), conf.paddingTop(),
                          sceneRect().width()-conf.paddingRight()-conf.paddingLeft(),
                          sceneRect().height()-conf.paddingBottom()-conf.paddingTop());
            sourceRect = QRectF(0.0, 0.0, r.width(), r.height());
        }
        else {
            r = QRectF(conf.paddingLeft(), conf.paddingTop(),
                          sceneRect().width(), sceneRect().height());
            sourceRect = r;
        }
    }
    frame->resizeFrame(r.size());

    // animation
    if (isAnimating() && frame->hasElementPrefix("decoration-inactive")) {
        QPixmap target = frame->framePixmap();
        frame->setElementPrefix("decoration-inactive");
        if (!isActive()) {
            frame->setElementPrefix("decoration");
        }
        if (!m_theme->isCompositingActive() && frame->hasElementPrefix("decoration-opaque-inactive")) {
            frame->setElementPrefix("decoration-opaque-inactive");
            if (!isActive()) {
                frame->setElementPrefix("decoration-opaque");
            }
        }
        if (maximized && frame->hasElementPrefix("decoration-maximized-inactive")) {
            frame->setElementPrefix("decoration-maximized-inactive");
            if (!isActive()) {
                frame->setElementPrefix("decoration-maximized");
            }
            if (!m_theme->isCompositingActive() && frame->hasElementPrefix("decoration-maximized-opaque-inactive")) {
                frame->setElementPrefix("decoration-maximized-opaque-inactive");
                if (!isActive()) {
                    frame->setElementPrefix("decoration-maximized-opaque");
                }
            }
        } else if (maximized && frame->hasElementPrefix("decoration-maximized")) {
            frame->setElementPrefix("decoration-maximized");
        }
        frame->resizeFrame(r.size());
        QPixmap result = Plasma::PaintUtils::transition(frame->framePixmap(),
                                                        target, m_animationProgress);
        painter->drawPixmap(r.toRect(), result, sourceRect);
    } else {
        frame->paintFrame(painter, r, sourceRect);
    }

    // inner border support
    if (frame->hasElementPrefix("innerborder") && !maximized) {
        if (!isActive() && frame->hasElementPrefix("innerborder-inactive")) {
            frame->setElementPrefix("innerborder-inactive");
        } else {
            frame->setElementPrefix("innerborder");
        }
        qreal leftMargin, topMargin, rightMargin, bottomMargin;
        frame->getMargins(leftMargin, topMargin, rightMargin, bottomMargin);
        int leftBorder, topBorder, rightBorder, bottomBorder;
        m_theme->borders(leftBorder, topBorder, rightBorder, bottomBorder, maximized);
        int leftPadding, topPadding, rightPadding, bottomPadding;
        m_theme->padding(leftPadding, topPadding, rightPadding, bottomPadding);
        qreal width = r.width() - leftBorder - rightBorder - leftPadding - rightPadding + leftMargin + rightMargin;
        qreal height = r.height() - topBorder - bottomBorder - topPadding - bottomPadding + topMargin + bottomMargin;
        QPointF point = QPointF(leftBorder + leftPadding - leftMargin, topBorder + topPadding - topMargin);
        frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);
        frame->resizeFrame(QSizeF(width, height));
        if (isAnimating() && frame->hasElementPrefix("innerborder-inactive")) {
            QPixmap target = frame->framePixmap();
            frame->setElementPrefix("innerborder-inactive");
            if (!isActive()) {
                frame->setElementPrefix("innerborder");
            }
            frame->resizeFrame(QSizeF(width, height));
            QPixmap result = Plasma::PaintUtils::transition(frame->framePixmap(),
                                                            target, m_animationProgress);
            painter->drawPixmap(point, result);
        } else {
            frame->paintFrame(painter, point);
        }
    }
    painter->restore();
}

void AuroraeScene::updateLayout()
{
    if (!m_theme->isValid()) {
        return;
    }
    const ThemeConfig &config = m_theme->themeConfig();
    int marginTop = qMin(config.buttonMarginTop(), config.titleHeight() - config.buttonHeight());
    marginTop = qMax(marginTop, 0);
    const int left  = config.paddingLeft();
    const int genericTop   = config.paddingTop() + marginTop;
    const int right = sceneRect().width() - m_rightButtons->preferredWidth() - config.paddingRight();
    const qreal titleHeight = qMax((qreal)config.titleHeight(),
                                   config.buttonHeight()*m_theme->buttonSizeFactor() + config.buttonMarginTop());
    DecorationPosition decoPos = (DecorationPosition)config.decorationPosition();
    if (isShade()) {
        decoPos = DecorationTop;
    }
    if (m_maximizeMode == KDecorationDefines::MaximizeFull) { // TODO: check option
        switch (decoPos) {
        case DecorationTop: {
            const int top = genericTop + config.titleEdgeTopMaximized();
            m_leftButtons->setGeometry(QRectF(QPointF(left + config.titleEdgeLeftMaximized(), top),
                                            m_leftButtons->size()));
            m_rightButtons->setGeometry(QRectF(QPointF(right - config.titleEdgeRightMaximized(), top),
                                            m_rightButtons->size()));
            // title
            const int leftTitle = m_leftButtons->geometry().right() + config.titleBorderLeft();
            const int titleWidth = m_rightButtons->geometry().left() - config.titleBorderRight() - leftTitle;
            m_title->setGeometry(leftTitle, config.paddingTop() + config.titleEdgeTopMaximized(),
                                titleWidth, titleHeight);
            break;
        }

        case DecorationBottom: {
            const int bottom = height() - config.paddingBottom() - marginTop - config.titleEdgeBottomMaximized();
            m_leftButtons->setGeometry(QRectF(QPointF(left + config.titleEdgeLeftMaximized(),
                                                      bottom - config.buttonHeight()),
                                              m_leftButtons->size()));
            m_rightButtons->setGeometry(QRectF(QPointF(right - config.titleEdgeRightMaximized(),
                                                      bottom - config.buttonHeight()),
                                              m_rightButtons->size()));
            // title
            const int leftTitle = m_leftButtons->geometry().right() + config.titleBorderLeft();
            const int titleWidth = m_rightButtons->geometry().left() - config.titleBorderRight() - leftTitle;
            m_title->setGeometry(leftTitle,
                                 height() - config.paddingBottom() - config.titleEdgeBottomMaximized() - titleHeight,
                                 titleWidth, titleHeight);
            break;
        }
        case DecorationLeft: {
            const int left = config.paddingLeft() + marginTop + config.titleEdgeLeftMaximized();
            m_rightButtons->setGeometry(left,
                                       height() - config.paddingBottom() - config.titleEdgeBottomMaximized() - m_rightButtons->preferredHeight(),
                                       m_rightButtons->preferredWidth(), m_rightButtons->preferredHeight());
            m_leftButtons->setGeometry(left, config.paddingTop() + config.titleEdgeTopMaximized(),
                                        m_leftButtons->preferredWidth(), m_leftButtons->preferredHeight());
            // title
            const int topTitle = m_leftButtons->geometry().bottom() + config.titleBorderRight();
            const int realTitleHeight = m_rightButtons->geometry().top() - config.titleBorderLeft() - topTitle;
            m_title->setGeometry(left, topTitle, titleHeight, realTitleHeight);
            break;
        }
        case DecorationRight: {
            const int left = width() - config.paddingRight() - marginTop - config.titleEdgeRightMaximized() - titleHeight;
            m_rightButtons->setGeometry(left,
                                       height() - config.paddingBottom() - config.titleEdgeBottomMaximized() - m_rightButtons->preferredHeight(),
                                       m_rightButtons->preferredWidth(), m_rightButtons->preferredHeight());
            m_leftButtons->setGeometry(left, config.paddingTop() + config.titleEdgeTopMaximized(),
                                        m_leftButtons->preferredWidth(), m_leftButtons->preferredHeight());
            // title
            const int topTitle = m_leftButtons->geometry().bottom() + config.titleBorderRight();
            const int realTitleHeight = m_rightButtons->geometry().top() - config.titleBorderLeft() - topTitle;
            m_title->setGeometry(left, topTitle, titleHeight, realTitleHeight);
            break;
        }
        }
        m_title->layout()->invalidate();
    } else {
        switch (decoPos) {
        case DecorationTop: {
            const int top = genericTop + config.titleEdgeTop();
            m_leftButtons->setGeometry(QRectF(QPointF(left + config.titleEdgeLeft(), top), m_leftButtons->size()));
            m_rightButtons->setGeometry(QRectF(QPointF(right - config.titleEdgeRight(), top), m_rightButtons->size()));
            // title
            const int leftTitle = m_leftButtons->geometry().right() + config.titleBorderLeft();
            const int titleWidth = m_rightButtons->geometry().left() - config.titleBorderRight() - leftTitle;
            m_title->setGeometry(leftTitle, config.paddingTop() + config.titleEdgeTop(),
                                titleWidth, titleHeight);
            break;
        }
        case DecorationBottom: {
            const int bottom = height() - config.paddingBottom() - marginTop - config.titleEdgeBottom();
            m_leftButtons->setGeometry(QRectF(QPointF(left + config.titleEdgeLeft(),
                                                      bottom - config.buttonHeight()),
                                              m_leftButtons->size()));
            m_rightButtons->setGeometry(QRectF(QPointF(right - config.titleEdgeRight(),
                                                      bottom - config.buttonHeight()),
                                              m_rightButtons->size()));
            // title
            const int leftTitle = m_leftButtons->geometry().right() + config.titleBorderLeft();
            const int titleWidth = m_rightButtons->geometry().left() - config.titleBorderRight() - leftTitle;
            m_title->setGeometry(leftTitle,
                                 height() - config.paddingBottom() - config.titleEdgeBottom() - titleHeight,
                                 titleWidth, titleHeight);
            break;
        }
        case DecorationLeft: {
            const int left = config.paddingLeft() + marginTop + config.titleEdgeLeft();
            m_rightButtons->setGeometry(left,
                                       height() - config.paddingBottom() - config.titleEdgeBottom() - m_rightButtons->preferredHeight(),
                                       m_rightButtons->preferredWidth(), m_rightButtons->preferredHeight());
            m_leftButtons->setGeometry(left, config.paddingTop() + config.titleEdgeTop(),
                                        m_leftButtons->preferredWidth(), m_leftButtons->preferredHeight());
            // title
            const int topTitle = m_leftButtons->geometry().bottom() + config.titleBorderRight();
            const int realTitleHeight = m_rightButtons->geometry().top() - config.titleBorderLeft() - topTitle;
            m_title->setGeometry(left, topTitle, titleHeight, realTitleHeight);
            break;
        }
        case DecorationRight: {
            const int left = width() - config.paddingRight() - marginTop - config.titleEdgeRight() - titleHeight;
            m_rightButtons->setGeometry(left,
                                       height() - config.paddingBottom() - config.titleEdgeBottom() - m_rightButtons->preferredHeight(),
                                       m_rightButtons->preferredWidth(), m_rightButtons->preferredHeight());
            m_leftButtons->setGeometry(left, config.paddingTop() + config.titleEdgeTop(),
                                        m_leftButtons->preferredWidth(), m_leftButtons->preferredHeight());
            // title
            const int topTitle = m_leftButtons->geometry().bottom() + config.titleBorderRight();
            const int realTitleHeight = m_rightButtons->geometry().top() - config.titleBorderLeft() - topTitle;
            m_title->setGeometry(left, topTitle, titleHeight, realTitleHeight);
            break;
        }
        }
        m_title->layout()->invalidate();
    }
}

void AuroraeScene::initButtons(QGraphicsLinearLayout* layout, const QString& buttons) const
{
    if (!m_theme->isValid()) {
        return;
    }
    foreach (const QChar &button, buttons) {
        switch (button.toAscii()) {
            case 'M': {
                AuroraeMenuButton *button = new AuroraeMenuButton(m_theme);
                if (m_theme->isShowTooltips()) {
                    button->setToolTip(i18n("Menu"));
                }
                connect(button, SIGNAL(clicked()), SIGNAL(menuClicked()));
                connect(button, SIGNAL(doubleClicked()), SIGNAL(menuDblClicked()));
                layout->addItem(button);
                break;
            }
            case 'S':
                if (m_theme->hasButton(AllDesktopsButton)) {
                    AuroraeButton *button = new AuroraeButton(m_theme, AllDesktopsButton);
                    button->setCheckable(true);
                    button->setChecked(m_allDesktops);
                    if (m_theme->isShowTooltips()) {
                        button->setToolTip(m_allDesktops?i18n("Not on all desktops"):i18n("On all desktops"));
                    }
                    connect(button, SIGNAL(clicked()), SIGNAL(toggleOnAllDesktops()));
                    layout->addItem(button);
                }
                break;
            case 'H':
                if (m_contextHelp && m_theme->hasButton(HelpButton)) {
                    AuroraeButton *button = new AuroraeButton(m_theme, HelpButton);
                    if (m_theme->isShowTooltips()) {
                        button->setToolTip(i18n("Help"));
                    }
                    connect(button, SIGNAL(clicked()), SIGNAL(showContextHelp()));
                    layout->addItem(button);
                }
                break;
            case 'I':
                if (m_theme->hasButton(MinimizeButton)) {
                    AuroraeButton *button = new AuroraeButton(m_theme, MinimizeButton);
                    if (m_theme->isShowTooltips()) {
                        button->setToolTip(i18n("Minimize"));
                    }
                    connect(button, SIGNAL(clicked()), SIGNAL(minimizeWindow()));
                    layout->addItem(button);
                }
                break;
            case 'A':
                if (m_theme->hasButton(MaximizeButton) || m_theme->hasButton(RestoreButton)) {
                    AuroraeMaximizeButton *button = new AuroraeMaximizeButton(m_theme);
                    button->setMaximizeMode(m_maximizeMode);
                    if (m_theme->isShowTooltips()) {
                        button->setToolTip(m_maximizeMode==KDecorationDefines::MaximizeFull?i18n("Restore"):i18n("Maximize") );
                    }
                    connect(button, SIGNAL(clicked(Qt::MouseButtons)), SIGNAL(maximize(Qt::MouseButtons)));
                    layout->addItem(button);
                }
                break;
            case 'X':
                if (m_theme->hasButton(CloseButton)){
                    AuroraeButton *button = new AuroraeButton(m_theme, CloseButton);
                    if (m_theme->isShowTooltips()) {
                        button->setToolTip(i18n("Close"));
                    }
                    connect(button, SIGNAL(clicked()), SIGNAL(closeWindow()));
                    layout->addItem(button);
                }
                break;
            case 'F':
                if (m_theme->hasButton(KeepAboveButton)) {
                    AuroraeButton *button = new AuroraeButton(m_theme, KeepAboveButton);
                    button->setCheckable(true);
                    button->setChecked(m_keepAbove);
                    if (m_theme->isShowTooltips()) {
                        button->setToolTip(m_keepAbove?i18n("Do not keep above others"):i18n("Keep above others"));
                    }
                    connect(button, SIGNAL(clicked()), SIGNAL(toggleKeepAbove()));
                    layout->addItem(button);
                }
                break;
            case 'B':
                if (m_theme->hasButton(KeepBelowButton)) {
                    AuroraeButton *button = new AuroraeButton(m_theme, KeepBelowButton);
                    button->setCheckable(true);
                    button->setChecked(m_keepBelow);
                    if (m_theme->isShowTooltips()) {
                        button->setToolTip(m_keepBelow?i18n("Do not keep below others"):i18n("Keep below others"));
                    }
                    connect(button, SIGNAL(clicked()), SIGNAL(toggleKeepBelow()));
                    layout->addItem(button);
                }
                break;
            case 'L':
                if (m_theme->hasButton(ShadeButton)) {
                    AuroraeButton *button = new AuroraeButton(m_theme, ShadeButton);
                    button->setCheckable(true);
                    button->setChecked(m_shade);
                    if (m_theme->isShowTooltips()) {
                        button->setToolTip(m_shade?i18n("Unshade"):i18n("Shade"));
                    }
                    connect(button, SIGNAL(clicked()), SIGNAL(toggleShade()));
                    layout->addItem(button);
                }
                break;
            case '_':
                layout->addItem(new AuroraeSpacer(m_theme));
                break;
            default:
                break; // nothing
        }
    }
}

void AuroraeScene::setIcon(const QIcon &icon)
{
    m_iconPixmap = icon;
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeMenuButton *button = dynamic_cast< AuroraeMenuButton* >(item)) {
            const int iconSize = qMin(button->size().width(), button->size().height());
            const QSize size = icon.actualSize(QSize(iconSize, iconSize));
            QPixmap pix = icon.pixmap(size);
            button->setIcon(pix);
        }
    }
}

bool AuroraeScene::isActive() const
{
    return m_active;
}

void AuroraeScene::setActive(bool active, bool animate)
{
    m_active = active;
    if (isAnimating()) {
        m_animation->stop();
    }
    m_animationProgress = 0.0;
    int time = m_theme->themeConfig().animationTime();
    if (time != 0 && animate) {
        m_animation->setDuration(time);
        m_animation->setEasingCurve(QEasingCurve::InOutQuad);
        m_animation->setStartValue(0.0);
        m_animation->setEndValue(1.0);
        m_animation->start();
    }
    emit activeChanged();
    update(sceneRect());
}

KDecorationDefines::MaximizeMode AuroraeScene::maximizeMode() const
{
    return m_maximizeMode;
}

void AuroraeScene::setMaximizeMode(KDecorationDefines::MaximizeMode mode)
{
    m_maximizeMode = mode;
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeMaximizeButton *button = dynamic_cast< AuroraeMaximizeButton* >(item)) {
            button->setMaximizeMode(mode);
            if (m_theme->isShowTooltips()) {
                button->setToolTip(m_maximizeMode==KDecorationDefines::MaximizeFull?i18n("Restore"):i18n("Maximize") );
            }
        }
    }
    updateLayout();
    update(sceneRect());
}

bool AuroraeScene::isAnimating() const
{
    return (m_animation->state() == QAbstractAnimation::Running);
}

qreal AuroraeScene::animationProgress() const
{
    return m_animationProgress;
}

void AuroraeScene::setAnimationProgress(qreal progress)
{
    m_animationProgress = progress;
    update(sceneRect());
}

bool AuroraeScene::isAllDesktops() const
{
    return m_allDesktops;
}

void AuroraeScene::setAllDesktops(bool all)
{
    if (m_allDesktops == all) {
        return;
    }
    m_allDesktops = all;
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeButton *button = dynamic_cast< AuroraeButton* >(item)) {
            if (button->type() == AllDesktopsButton) {
                button->setChecked(m_allDesktops);
                if (m_theme->isShowTooltips()) {
                    button->setToolTip(m_allDesktops?i18n("Not on all desktops"):i18n("On all desktops"));
                }
                button->update();
            }
        }
    }
}

bool AuroraeScene::isKeepAbove() const
{
    return m_keepAbove;
}

void AuroraeScene::setKeepAbove(bool keep)
{
    if (m_keepAbove == keep) {
        return;
    }
    m_keepAbove = keep;
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeButton *button = dynamic_cast< AuroraeButton* >(item)) {
            if (button->type() == KeepAboveButton) {
                button->setChecked(m_keepAbove);
                if (m_theme->isShowTooltips()) {
                    button->setToolTip(m_keepAbove?i18n("Do not keep above others"):i18n("Keep above others"));
                }
                button->update();
            }
        }
    }
}

bool AuroraeScene::isKeepBelow() const
{
    return m_keepBelow;
}

void AuroraeScene::setKeepBelow(bool keep)
{
    if (m_keepBelow == keep) {
        return;
    }
    m_keepBelow = keep;
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeButton *button = dynamic_cast< AuroraeButton* >(item)) {
            if (button->type() == KeepBelowButton) {
                button->setChecked(m_keepBelow);
                if (m_theme->isShowTooltips()) {
                    button->setToolTip(m_keepBelow?i18n("Do not keep below others"):i18n("Keep below others"));
                }
                button->update();
            }
        }
    }
}

bool AuroraeScene::isShade() const
{
    return m_shade;
}

void AuroraeScene::setShade(bool shade)
{
    if (m_shade == shade) {
        return;
    }
    m_shade = shade;
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeButton *button = dynamic_cast< AuroraeButton* >(item)) {
            if (button->type() == ShadeButton) {
                button->setChecked(m_shade);
                if (m_theme->isShowTooltips()) {
                    button->setToolTip(m_shade?i18n("Unshade"):i18n("Shade"));
                }
                button->update();
            }
        }
    }
    if ((DecorationPosition)m_theme->themeConfig().decorationPosition() != DecorationTop) {
        Qt::Orientation orientation = Qt::Horizontal;
        switch ((DecorationPosition)m_theme->themeConfig().decorationPosition()) {
            case DecorationLeft: // fall through
            case DecorationRight:
                orientation = Qt::Vertical;
                break;
            case DecorationTop: // fall through
            case DecorationBottom: // fall through
            default: // fall through
                orientation = Qt::Horizontal;
                break;
        }
        if (m_shade) {
            orientation = Qt::Horizontal;
        }
        static_cast<QGraphicsLinearLayout *>(m_rightButtons->layout())->setOrientation(orientation);
        static_cast<QGraphicsLinearLayout *>(m_leftButtons->layout())->setOrientation(orientation);
        static_cast<QGraphicsLinearLayout *>(m_title->layout())->setOrientation(orientation);
        updateLayout();
    }
}

int AuroraeScene::leftButtonsWidth() const
{
    if (!m_leftButtons) {
        return 0;
    }
    return m_leftButtons->preferredWidth();
}

int AuroraeScene::rightButtonsWidth() const
{
    if (!m_rightButtons) {
        return 0;
    }
    return m_rightButtons->preferredWidth();
}

void AuroraeScene::setButtons(const QString &left, const QString &right)
{
    m_leftButtonOrder = left;
    m_rightButtonOrder = right;
    resetTheme();
}

void AuroraeScene::setCaption(const QString &caption, int index)
{
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeTab *tab = dynamic_cast<AuroraeTab*>(item)) {
            if (tab->index() == index) {
                tab->setCaption(caption);
            }
        }
    }
}

void AuroraeScene::setCaptions(const QStringList &captions)
{
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeTab *tab = dynamic_cast<AuroraeTab*>(item)) {
            if (tab->index() < captions.size()) {
                tab->setCaption(captions[tab->index()]);
            }
        }
    }
}

void AuroraeScene::addTab(const QString &caption)
{
    AuroraeTab *tab = new AuroraeTab(m_theme, caption, m_tabCount);
    ++m_tabCount;
    connect(this, SIGNAL(activeChanged()), tab, SLOT(activeChanged()));
    connect(tab, SIGNAL(mouseButtonPress(QGraphicsSceneMouseEvent*,int)),
            SIGNAL(tabMouseButtonPress(QGraphicsSceneMouseEvent*,int)));
    connect(tab, SIGNAL(mouseButtonRelease(QGraphicsSceneMouseEvent*,int)),
            SIGNAL(tabMouseButtonRelease(QGraphicsSceneMouseEvent*,int)));
    connect(tab, SIGNAL(mouseDblClicked()), SIGNAL(titleDoubleClicked()));
    connect(tab, SIGNAL(tabRemoved(int)), SIGNAL(tabRemoved(int)));
    static_cast<QGraphicsLinearLayout*>(m_title->layout())->addItem(tab);
    tab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_title->layout()->invalidate();
}

void AuroraeScene::addTabs(const QStringList &captions)
{
    foreach (const QString &caption, captions) {
        addTab(caption);
    }
}

void AuroraeScene::removeLastTab()
{
    if (m_tabCount < 2) {
        return;
    }
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeTab *tab = dynamic_cast<AuroraeTab*>(item)) {
            if (tab->index() == m_tabCount-1) {
                m_title->layout()->removeAt(tab->index());
                removeItem(tab);
                --m_tabCount;
                m_title->layout()->invalidate();
                return;
            }
        }
    }
}

int AuroraeScene::tabCount() const
{
    return m_tabCount;
}

int AuroraeScene::focusedTab() const
{
    return m_focusedTab;
}

void AuroraeScene::setFocusedTab(int index)
{
    if (m_focusedTab != index) {
        m_focusedTab = index;
        m_title->update();
    }
}

bool AuroraeScene::isFocusedTab(int index) const
{
    if (m_tabCount == 1) {
        return true;
    }
    return (index == m_focusedTab);
}

void AuroraeScene::setUniqueTabDragId(int index, long int id)
{
    for (int i=0; i<m_title->layout()->count(); ++i) {
        if (static_cast<AuroraeTab*>(m_title->layout()->itemAt(i))->index() == index) {
            static_cast<AuroraeTab*>(m_title->layout()->itemAt(i))->setUniqueTabId(id);
            break;
        }
    }
}

void AuroraeScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mousePressEvent(event);
    if (!event->isAccepted()) {
        event->accept();
        emit titlePressed(event->button(), event->buttons());
    }
}

void AuroraeScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    if (!event->isAccepted()) {
        if (m_dblClicked && event->button() == Qt::LeftButton) {
            // eat event
            m_dblClicked = false;
            return;
        }
        emit titleReleased(event->button(), event->buttons());
    }
}

void AuroraeScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mouseDoubleClickEvent(event);
    if (!event->isAccepted() && event->button() == Qt::LeftButton) {
        m_dblClicked = true;
        emit titleDoubleClicked();
    }
}

void AuroraeScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mouseMoveEvent(event);
    emit titleMouseMoved(event->button(), event->buttons());
}

void AuroraeScene::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    QGraphicsScene::wheelEvent(event);
    if (!event->isAccepted()) {
        emit wheelEvent(event->delta());
    }
}

void AuroraeScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    QGraphicsScene::dragEnterEvent(event);
    if (event->mimeData()->hasFormat(m_theme->tabDragMimeType())) {
        event->accept();
    }
}

void AuroraeScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    QGraphicsScene::dragMoveEvent(event);
    if (event->mimeData()->hasFormat(m_theme->tabDragMimeType())) {
        event->accept();
    }
}

void AuroraeScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    QGraphicsScene::dropEvent(event);
    if (event->mimeData()->hasFormat(m_theme->tabDragMimeType())) {
        QList<QByteArray> ids = event->mimeData()->data(m_theme->tabDragMimeType()).split('/');
        bool insideScene = false;
        foreach (QGraphicsView *view, views()) {
            if (view->window() == event->source()->window()) {
                insideScene = true;
                break;
            }
        }
        int index = -1;
        if (QGraphicsItem *item = itemAt(event->scenePos())) {
            if (AuroraeTab *tab = dynamic_cast<AuroraeTab*>(item)) {
                index = tab->index();
            }
        }
        if (insideScene) {
            // move inside decoration
            emit tabMoved(ids[1].toInt(), index);
        } else {
            // move to this decoration
            long source = ids[0].toLong();
            emit tabMovedToGroup(source, index);
        }
    }
}

QString AuroraeScene::buttonsToDirection(const QString &buttons)
{
    QString ret;
    if (QApplication::layoutDirection() == Qt::RightToLeft) {
        // Qt swaps the buttons, so we have to swap them to be consistent with other KWin decos
        foreach (QChar c, buttons) {
            ret.prepend(c);
        }
    } else {
        ret = buttons;
    }
    return ret;
}

void AuroraeScene::showTooltipsChanged(bool show)
{
    foreach (QGraphicsItem *item, items()) {
        if (AuroraeButton *button = dynamic_cast< AuroraeButton* >(item)) {
            if (show) {
                // switch over type
                switch (button->type()) {
                case MenuButton:
                    button->setToolTip(i18n("Menu"));
                    break;
                case MinimizeButton:
                    button->setToolTip(i18n("Minimize"));
                    break;
                case RestoreButton: // fall through
                case MaximizeButton:
                    button->setToolTip(m_maximizeMode==KDecorationDefines::MaximizeFull?i18n("Restore"):i18n("Maximize") );
                    break;
                case KeepAboveButton:
                    button->setToolTip(m_keepAbove?i18n("Do not keep above others"):i18n("Keep above others"));
                    break;
                case KeepBelowButton:
                    button->setToolTip(m_keepBelow?i18n("Do not keep below others"):i18n("Keep below others"));
                    break;
                case ShadeButton:
                    button->setToolTip(m_shade?i18n("Unshade"):i18n("Shade"));
                    break;
                case CloseButton:
                    button->setToolTip(i18n("Close"));
                    break;
                case AllDesktopsButton:
                    button->setToolTip(m_allDesktops?i18n("Not on all desktops"):i18n("On all desktops"));
                    break;
                case HelpButton:
                    button->setToolTip(i18n("Help"));
                    break;
                default:
                    // nothing
                    break;
                }
            } else {
                button->setToolTip(QString());
            }
        }
    }
}

const QString& AuroraeScene::leftButtons() const
{
    return m_leftButtonOrder;
}

const QString &AuroraeScene::rightButtons() const
{
    return m_rightButtonOrder;
}

} // namespace
