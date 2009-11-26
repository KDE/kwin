/********************************************************************
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "aurorae.h"

#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>

#include <KConfig>
#include <KConfigGroup>
#include <KDE/Plasma/Animator>
#include <KDE/Plasma/PaintUtils>
#include <KIconEffect>
#include <KIconLoader>

#include <QPainter>

namespace Aurorae
{

AuroraeFactory::AuroraeFactory()
        : QObject()
        , KDecorationFactoryUnstable()
{
    init();
}

void AuroraeFactory::init()
{
    KConfig conf("auroraerc");
    KConfigGroup group(&conf, "Engine");

    m_themeName = group.readEntry("ThemeName", "example-deco");

    QString file("aurorae/themes/" + m_themeName + "/decoration.svg");
    QString path = KGlobal::dirs()->findResource("data", file);
    if (path.isEmpty()) {
        file += 'z';
        path = KGlobal::dirs()->findResource("data", file);
    }
    if (path.isEmpty()) {
        kDebug(1216) << "Could not find decoration svg: aborting";
        abort();
    }
    m_frame.setImagePath(path);
    m_frame.setCacheAllRenderedFrames(true);
    m_frame.setEnabledBorders(Plasma::FrameSvg::AllBorders);

    // load the buttons
    initButtonFrame("minimize");
    initButtonFrame("maximize");
    initButtonFrame("restore");
    initButtonFrame("close");
    initButtonFrame("alldesktops");
    initButtonFrame("keepabove");
    initButtonFrame("keepbelow");
    initButtonFrame("shade");
    initButtonFrame("help");

    readThemeConfig();
}

void AuroraeFactory::readThemeConfig()
{
    // read config values
    KConfig conf("aurorae/themes/" + m_themeName + '/' + m_themeName + "rc", KConfig::FullConfig, "data");
    m_themeConfig.load(&conf);
}

AuroraeFactory::~AuroraeFactory()
{
    s_instance = NULL;
}

AuroraeFactory *AuroraeFactory::instance()
{
    if (!s_instance) {
        s_instance = new AuroraeFactory;
    }

    return s_instance;
}

bool AuroraeFactory::reset(unsigned long changed)
{
    // re-read config
    m_frame.clearCache();
    m_buttons.clear();
    init();
    resetDecorations(changed);
    return false; // need hard reset
}

bool AuroraeFactory::supports(Ability ability) const
{
    switch (ability) {
    case AbilityAnnounceButtons:
    case AbilityUsesAlphaChannel:
    case AbilityButtonMenu:
    case AbilityButtonSpacer:
        return true;
    case AbilityButtonMinimize:
        return m_buttons.contains("minimize");
    case AbilityButtonMaximize:
        return m_buttons.contains("maximize") || m_buttons.contains("restore");
    case AbilityButtonClose:
        return m_buttons.contains("close");
    case AbilityButtonAboveOthers:
        return m_buttons.contains("keepabove");
    case AbilityButtonBelowOthers:
        return m_buttons.contains("keepbelow");
    case AbilityButtonShade:
        return m_buttons.contains("shade");
    case AbilityButtonOnAllDesktops:
        return m_buttons.contains("alldesktops");
    case AbilityButtonHelp:
        return m_buttons.contains("help");
    case AbilityProvidesShadow:
        return m_themeConfig.shadow();
    default:
        return false;
    }
}

KDecoration *AuroraeFactory::createDecoration(KDecorationBridge *bridge)
{
    AuroraeClient *client = new AuroraeClient(bridge, this);
    return client->decoration();
}

void AuroraeFactory::initButtonFrame(const QString &button)
{
    QString file("aurorae/themes/" + m_themeName + '/' + button + ".svg");
    QString path = KGlobal::dirs()->findResource("data", file);
    if (path.isEmpty()) {
        // let's look for svgz
        file.append("z");
        path = KGlobal::dirs()->findResource("data", file);
    }
    if (!path.isEmpty()) {
        Plasma::FrameSvg *frame = new Plasma::FrameSvg(this);
        frame->setImagePath(path);
        frame->setCacheAllRenderedFrames(true);
        frame->setEnabledBorders(Plasma::FrameSvg::NoBorder);
        m_buttons[ button ] = frame;
    } else {
        kDebug(1216) << "No button for: " << button;
    }
}

Plasma::FrameSvg *AuroraeFactory::button(const QString &b)
{
    if (hasButton(b)) {
        return m_buttons[ b ];
    } else {
        return NULL;
    }
}

QList< KDecorationDefines::BorderSize > AuroraeFactory::borderSizes() const
{
    return QList< BorderSize >() << BorderTiny << BorderNormal <<
        BorderLarge << BorderVeryLarge <<  BorderHuge <<
        BorderVeryHuge << BorderOversized;
}


AuroraeFactory *AuroraeFactory::s_instance = NULL;

/*******************************************************
* Button
*******************************************************/
AuroraeButton::AuroraeButton(ButtonType type, KCommonDecoration *parent)
        : KCommonDecorationButton(type, parent)
        , m_animationId(0)
        , m_animationProgress(0.0)
        , m_pressed(false)
{
    setAttribute(Qt::WA_NoSystemBackground);
    connect(Plasma::Animator::self(), SIGNAL(customAnimationFinished(int)), this, SLOT(animationFinished(int)));
}

void AuroraeButton::reset(unsigned long changed)
{
    Q_UNUSED(changed)
}

void AuroraeButton::enterEvent(QEvent *event)
{
    Q_UNUSED(event)
    if (m_animationId != 0) {
        Plasma::Animator::self()->stopCustomAnimation(m_animationId);
    }
    m_animationProgress = 0.0;
    int time = AuroraeFactory::instance()->themeConfig().animationTime();
    if (time != 0) {
        m_animationId = Plasma::Animator::self()->customAnimation(40 / (1000.0 / qreal(time)),
                        time, Plasma::Animator::EaseInCurve, this, "animationUpdate");
    }
    update();
}

void AuroraeButton::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    if (m_animationId != 0) {
        Plasma::Animator::self()->stopCustomAnimation(m_animationId);
    }
    m_animationProgress = 0.0;
    int time = AuroraeFactory::instance()->themeConfig().animationTime();
    if (time != 0) {
        m_animationId = Plasma::Animator::self()->customAnimation(40 / (1000.0 / qreal(time)),
                        time, Plasma::Animator::EaseOutCurve, this, "animationUpdate");
    }
    update();
}

void AuroraeButton::mousePressEvent(QMouseEvent *e)
{
    m_pressed = true;
    update();
    KCommonDecorationButton::mousePressEvent(e);
}

void AuroraeButton::mouseReleaseEvent(QMouseEvent *e)
{
    m_pressed = false;
    update();
    KCommonDecorationButton::mouseReleaseEvent(e);
}

void AuroraeButton::animationUpdate(qreal progress, int id)
{
    Q_UNUSED(id)
    m_animationProgress = progress;
    update();
}

void AuroraeButton::animationFinished(int id)
{
    if (m_animationId == id) {
        m_animationId = 0;
        update();
    }
}

void AuroraeButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (decoration()->isPreview()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool active = decoration()->isActive();

    if (type() == MenuButton) {
        const QIcon icon = decoration()->icon();
        ThemeConfig config = AuroraeFactory::instance()->themeConfig();
        int iconSize = qMin(config.buttonWidthMenu(), config.buttonHeight());
        const QSize size = icon.actualSize(QSize(iconSize, iconSize));
        QPixmap iconPix = icon.pixmap(size);
        KIconEffect *effect = KIconLoader::global()->iconEffect();
        if (active) {
            if (underMouse()) {
                iconPix = effect->apply(iconPix, KIconLoader::Desktop, KIconLoader::ActiveState);
            }
        } else {
            iconPix = effect->apply(iconPix, KIconLoader::Desktop, KIconLoader::DisabledState);
        }
        AuroraeClient *deco = dynamic_cast<AuroraeClient*>(decoration());
        if (deco && deco->isAnimating()) {
            // animation
            QPixmap oldPix = icon.pixmap(size);
            if (!active) {
                if (underMouse()) {
                    oldPix = effect->apply(iconPix, KIconLoader::Desktop, KIconLoader::ActiveState);
                }
            } else {
                oldPix = effect->apply(iconPix, KIconLoader::Desktop, KIconLoader::DisabledState);
            }
            iconPix = Plasma::PaintUtils::transition(oldPix, iconPix, deco->animationProgress());
        }
        painter.drawPixmap(0, 0, iconPix);
        return;
    }

    ButtonStates states;
    if (active) {
        states |= Active;
    }
    if (underMouse()) {
        states |= Hover;
    }
    if (m_pressed) {
        states |= Pressed;
    }
    QString buttonName = "";
    switch (type()) {
    case MinButton:
        if (!decoration()->isMinimizable()) {
            states |= Deactivated;
        }
        buttonName = "minimize";
        break;
    case CloseButton:
        if (!decoration()->isCloseable()) {
            states |= Deactivated;
        }
        buttonName = "close";
        break;
    case MaxButton: {
        if (!decoration()->isMaximizable()) {
            states |= Deactivated;
        }
        buttonName = "maximize";
        if (decoration()->maximizeMode() == KDecorationDefines::MaximizeFull &&
                !decoration()->options()->moveResizeMaximizedWindows()) {
            buttonName = "restore";
            if (!AuroraeFactory::instance()->hasButton(buttonName)) {
                buttonName = "maximize";
            }
        }
        break;
    }
    case OnAllDesktopsButton:
        if (decoration()->isOnAllDesktops()) {
            states |= Hover;
        }
        buttonName = "alldesktops";
        break;
    case AboveButton:
        buttonName = "keepabove";
        break;
    case BelowButton:
        buttonName = "keepbelow";
        break;
    case ShadeButton:
        if (!decoration()->isShadeable()) {
            states |= Deactivated;
        }
        if (decoration()->isShade()) {
            states |= Hover;
        }
        buttonName = "shade";
        break;
    case HelpButton:
        buttonName = "help";
        break;
    default:
        buttonName.clear();
    }

    if (!buttonName.isEmpty()) {
        if (AuroraeFactory::instance()->hasButton(buttonName)) {
            Plasma::FrameSvg *button = AuroraeFactory::instance()->button(buttonName);
            paintButton(painter, button, states);
        }
    }
}

void AuroraeButton::paintButton(QPainter &painter, Plasma::FrameSvg *frame, ButtonStates states)
{
    QString prefix = "active";
    QString animationPrefix = "active";
    bool hasInactive = false;
    // check for inactive prefix
    if (!states.testFlag(Active) && frame->hasElementPrefix("inactive")) {
        // we have inactive, so we use it
        hasInactive = true;
        prefix = "inactive";
        animationPrefix = "inactive";
    }

    if (states.testFlag(Hover)) {
        if (states.testFlag(Active)) {
            if (frame->hasElementPrefix("hover")) {
                prefix = "hover";
            }
        } else {
            if (hasInactive) {
                if (frame->hasElementPrefix("hover-inactive")) {
                    prefix = "hover-inactive";
                }
            } else {
                if (frame->hasElementPrefix("hover")) {
                    prefix = "hover";
                }
            }
        }
    }
    if (states.testFlag(Pressed)) {
        if (states.testFlag(Active)) {
            if (frame->hasElementPrefix("pressed")) {
                prefix = "pressed";
            }
        } else {
            if (hasInactive) {
                if (frame->hasElementPrefix("pressed-inactive")) {
                    prefix = "pressed-inactive";
                }
            } else {
                if (frame->hasElementPrefix("pressed")) {
                    prefix = "pressed";
                }
            }
        }
    }
    if (states.testFlag(Deactivated)) {
        if (states.testFlag(Active)) {
            if (frame->hasElementPrefix("deactivated")) {
                prefix = "deactivated";
            }
        } else {
            if (hasInactive) {
                if (frame->hasElementPrefix("deactivated-inactive")) {
                    prefix = "deactivated-inactive";
                }
            } else {
                if (frame->hasElementPrefix("deactivated")) {
                    prefix = "deactivated";
                }
            }
        }
    }
    frame->setElementPrefix(prefix);
    frame->resizeFrame(size());
    if (m_animationId != 0) {
        // there is an animation so we have to use it
        // the animation is definately a hover animation as currently nothing else is supported
        if (!states.testFlag(Hover)) {
            // only have to set for not hover state as animationPrefix is set to (in)active by default
            if (states.testFlag(Active)) {
                if (frame->hasElementPrefix("hover")) {
                    animationPrefix = "hover";
                }
            } else {
                if (hasInactive) {
                    if (frame->hasElementPrefix("hover-inactive")) {
                        animationPrefix = "hover-inactive";
                    }
                } else {
                    if (frame->hasElementPrefix("hover")) {
                        animationPrefix = "hover";
                    }
                }
            }
        }
        QPixmap target = frame->framePixmap();
        frame->setElementPrefix(animationPrefix);
        frame->resizeFrame(size());
        QPixmap result = Plasma::PaintUtils::transition(frame->framePixmap(),
                                                         target, m_animationProgress);
        painter.drawPixmap(rect(), result);
    } else {
        bool animation = false;
        AuroraeClient *deco = dynamic_cast<AuroraeClient*>(decoration());
        if (deco && deco->isAnimating()) {
            animationPrefix = prefix;
            if (prefix.endsWith("-inactive")) {
                animationPrefix.remove("-inactive");
            } else {
                animationPrefix = animationPrefix + "-inactive";
            }
            if (frame->hasElementPrefix(animationPrefix)) {
                animation = true;
                QPixmap target = frame->framePixmap();
                frame->setElementPrefix(animationPrefix);
                frame->resizeFrame(size());
                QPixmap result = Plasma::PaintUtils::transition(frame->framePixmap(),
                                                                target, deco->animationProgress());
                painter.drawPixmap(rect(), result);
            }
        }
        if (!animation) {
            frame->paintFrame(&painter);
        }
    }
}

/*******************************************************
* Client
*******************************************************/

AuroraeClient::AuroraeClient(KDecorationBridge *bridge, KDecorationFactory *factory)
        : KCommonDecorationUnstable(bridge, factory)
{
    connect(Plasma::Animator::self(), SIGNAL(customAnimationFinished(int)), this, SLOT(animationFinished(int)));
}

AuroraeClient::~AuroraeClient()
{
}

void AuroraeClient::init()
{
    KCommonDecoration::init();
}

void AuroraeClient::reset(unsigned long changed)
{
    if (changed & SettingCompositing) {
        updateWindowShape();
    }
    widget()->update();

    KCommonDecoration::reset(changed);
}

QString AuroraeClient::visibleName() const
{
    return i18n("Aurorae Theme Engine");
}

QString AuroraeClient::defaultButtonsLeft() const
{
    return AuroraeFactory::instance()->themeConfig().defaultButtonsLeft();
}

QString AuroraeClient::defaultButtonsRight() const
{
    return AuroraeFactory::instance()->themeConfig().defaultButtonsRight();
}

bool AuroraeClient::decorationBehaviour(DecorationBehaviour behavior) const
{
    switch (behavior) {
    case DB_MenuClose:
        return true; // Close on double click

    case DB_WindowMask:
    case DB_ButtonHide:
        return false;
    default:
        return false;
    }
}

int AuroraeClient::layoutMetric(LayoutMetric lm, bool respectWindowState,
                                   const KCommonDecorationButton *button) const
{
    bool maximized = maximizeMode() == MaximizeFull &&
                     !options()->moveResizeMaximizedWindows();
    const ThemeConfig &conf = AuroraeFactory::instance()->themeConfig();
    qreal left, top, right, bottom;
    AuroraeFactory::instance()->frame()->getMargins(left, top, right, bottom);
    int borderLeft, borderRight, borderBottom;;
    switch (KDecoration::options()->preferredBorderSize(AuroraeFactory::instance())) {
    case BorderTiny:
        if (compositingActive()) {
            borderLeft = qMin(0, (int)left - conf.borderLeft() - conf.paddingLeft());
            borderRight = qMin(0, (int)right - conf.borderRight() - conf.paddingRight());
            borderBottom = qMin(0, (int)bottom - conf.borderBottom() - conf.paddingBottom());
        } else {
            borderLeft = qMin(0, (int)left - conf.borderLeft());
            borderRight = qMin(0, (int)right - conf.borderRight());
            borderBottom = qMin(0, (int)bottom - conf.borderBottom());
        }
        break;
    case BorderLarge:
        borderLeft = borderRight = borderBottom = 4;
        break;
    case BorderVeryLarge:
        borderLeft = borderRight = borderBottom = 8;
        break;
    case BorderHuge:
        borderLeft = borderRight = borderBottom = 12;
        break;
    case BorderVeryHuge:
        borderLeft = borderRight = borderBottom = 23;
        break;
    case BorderOversized:
        borderLeft = borderRight = borderBottom = 36;
        break;
    case BorderNormal:
    default:
        borderLeft = borderRight = borderBottom =  0;
    }
    switch (lm) {
    case LM_BorderLeft:
        return maximized && respectWindowState ? 0 : conf.borderLeft() + borderLeft;
    case LM_BorderRight:
        return maximized && respectWindowState ? 0 : conf.borderRight() + borderRight;
    case LM_BorderBottom:
        return maximized && respectWindowState ? 0 : conf.borderBottom() + borderBottom;

    case LM_OuterPaddingLeft:
        return conf.paddingLeft();
    case LM_OuterPaddingRight:
        return conf.paddingRight();
    case LM_OuterPaddingTop:
        return conf.paddingTop();
    case LM_OuterPaddingBottom:
        return conf.paddingBottom();

    case LM_TitleEdgeLeft:
        return maximized && respectWindowState ? conf.titleEdgeLeftMaximized() : conf.titleEdgeLeft();
    case LM_TitleEdgeRight:
        return maximized && respectWindowState ? conf.titleEdgeRightMaximized() : conf.titleBorderRight();
    case LM_TitleEdgeTop:
        return maximized && respectWindowState ? conf.titleEdgeTopMaximized() : conf.titleEdgeTop();
    case LM_TitleEdgeBottom:
        return maximized && respectWindowState ? conf.titleEdgeBottomMaximized() : conf.titleEdgeBottom();

    case LM_ButtonMarginTop:
        return conf.buttonMarginTop();

    case LM_TitleBorderLeft:
        return conf.titleBorderLeft();
    case LM_TitleBorderRight:
        return conf.titleBorderRight();
    case LM_TitleHeight:
        return conf.titleHeight();

    case LM_ButtonWidth:
        switch (button->type()) {
        case MinButton:
            return conf.buttonWidthMinimize();
        case MaxButton:
            return conf.buttonWidthMaximizeRestore();
        case CloseButton:
            return conf.buttonWidthClose();
        case OnAllDesktopsButton:
            return conf.buttonWidthAllDesktops();
        case AboveButton:
            return conf.buttonWidthKeepAbove();
        case BelowButton:
            return conf.buttonWidthKeepBelow();
        case ShadeButton:
            return conf.buttonWidthShade();
        case MenuButton:
            return conf.buttonWidthMenu();
        default:
            return conf.buttonWidth();
        }
    case LM_ButtonHeight:
        return conf.buttonHeight();
    case LM_ButtonSpacing:
        return conf.buttonSpacing();
    case LM_ExplicitButtonSpacer:
        return conf.explicitButtonSpacer();

    default:
        return KCommonDecoration::layoutMetric(lm, respectWindowState, button);
    }
}

KCommonDecorationButton *AuroraeClient::createButton(ButtonType type)
{
    AuroraeFactory *factory = AuroraeFactory::instance();
    switch (type) {
    case MenuButton:
        return new AuroraeButton(type, this);
    case MinButton:
        if (factory->hasButton("minimize")) {
            return new AuroraeButton(type, this);
        } else {
            return NULL;
        }
    case MaxButton:
        if (factory->hasButton("maximize") || factory->hasButton("restore")) {
            return new AuroraeButton(type, this);
        } else {
            return NULL;
        }
    case CloseButton:
        if (factory->hasButton("close")) {
            return new AuroraeButton(type, this);
        } else {
            return NULL;
        }
    case OnAllDesktopsButton:
        if (factory->hasButton("alldesktops")) {
            return new AuroraeButton(type, this);
        } else {
            return NULL;
        }
    case HelpButton:
        if (factory->hasButton("help")) {
            return new AuroraeButton(type, this);
        } else {
            return NULL;
        }
    case AboveButton:
        if (factory->hasButton("keepabove")) {
            return new AuroraeButton(type, this);
        } else {
            return NULL;
        }
    case BelowButton:
        if (factory->hasButton("keepbelow")) {
            return new AuroraeButton(type, this);
        } else {
            return NULL;
        }
    case ShadeButton:
        if (factory->hasButton("shade")) {
            return new AuroraeButton(type, this);
        } else {
            return NULL;
        }
    default:
        return NULL;
    }
}

void AuroraeClient::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (isPreview()) {
        return;
    }
    bool maximized = maximizeMode() == MaximizeFull &&
                     !options()->moveResizeMaximizedWindows();

    QPainter painter(widget());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    const ThemeConfig &conf = AuroraeFactory::instance()->themeConfig();
    Plasma::FrameSvg *frame = AuroraeFactory::instance()->frame();

    frame->setElementPrefix("decoration");
    if (!isActive() && frame->hasElementPrefix("decoration-inactive")) {
        frame->setElementPrefix("decoration-inactive");
    }
    if (!compositingActive() && frame->hasElementPrefix("decoration-opaque")) {
        frame->setElementPrefix("decoration-opaque");
        if (!isActive() && frame->hasElementPrefix("decoration-opaque-inactive")) {
            frame->setElementPrefix("decoration-opaque-inactive");
        }
    }

    // restrict painting on the decoration - no need to paint behind the window
    int left, right, top, bottom;
    decoration()->borders(left, right, top, bottom);
    if (!compositingActive() && !transparentRect().isNull()) {
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
    }

    // top
    if (maximized) {
        frame->setEnabledBorders(Plasma::FrameSvg::NoBorder);
    } else {
        frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    }
    QRectF rect = QRectF(0.0, 0.0, widget()->width(), widget()->height());
    if (maximized) {
        rect = QRectF(conf.paddingLeft(), conf.paddingTop(),
                      widget()->width() - conf.paddingRight(),
                      widget()->height() - conf.paddingBottom());
    }
    QRectF sourceRect = rect;
    if (!compositingActive()) {
        if (frame->hasElementPrefix("decoration-opaque")) {
            rect = QRectF(conf.paddingLeft(), conf.paddingTop(),
                          widget()->width()-conf.paddingRight()-conf.paddingLeft(),
                          widget()->height()-conf.paddingBottom()-conf.paddingTop());
            sourceRect = QRectF(0.0, 0.0, rect.width(), rect.height());
        }
        else {
            rect = QRectF(conf.paddingLeft(), conf.paddingTop(), widget()->width(), widget()->height());
            sourceRect = rect;
        }
    }
    frame->resizeFrame(rect.size());

    // animation
    if (m_animationId != 0 && frame->hasElementPrefix("decoration-inactive")) {
        QPixmap target = frame->framePixmap();
        frame->setElementPrefix("decoration-inactive");
        if (!isActive()) {
            frame->setElementPrefix("decoration");
        }
        if (!compositingActive() && frame->hasElementPrefix("decoration-opaque-inactive")) {
            frame->setElementPrefix("decoration-opaque-inactive");
            if (!isActive()) {
                frame->setElementPrefix("decoration-opaque");
            }
        }
        frame->resizeFrame(rect.size());
        QPixmap result = Plasma::PaintUtils::transition(frame->framePixmap(),
                                                        target, m_animationProgress);
        painter.drawPixmap(rect.toRect(), result);
    } else {
        frame->paintFrame(&painter, rect, sourceRect);
    }

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (m_animationId != 0) {
        QPixmap result;
        if (isActive()) {
            result = Plasma::PaintUtils::transition(m_inactiveText, m_activeText, m_animationProgress);
        } else {
            result = Plasma::PaintUtils::transition(m_activeText, m_inactiveText, m_animationProgress);
        }
        painter.drawPixmap(titleRect(), result);
    } else {
        if (isActive()) {
            painter.drawPixmap(titleRect(), m_activeText);
        } else {
            painter.drawPixmap(titleRect(), m_inactiveText);
            }
    }
}

void AuroraeClient::generateTextPixmap(QPixmap& pixmap, bool active)
{
    QPainter painter(&pixmap);
    pixmap.fill(Qt::transparent);
    const ThemeConfig &conf = AuroraeFactory::instance()->themeConfig();
    painter.setFont(options()->font(active));
    if (conf.useTextShadow()) {
        // shadow code is inspired by Qt FAQ: How can I draw shadows behind text?
        // see http://www.qtsoftware.com/developer/faqs/faq.2007-07-27.3052836051
        const ThemeConfig &conf = AuroraeFactory::instance()->themeConfig();
        painter.save();
        if (active) {
            painter.setPen(conf.activeTextShadowColor());
        }
        else {
            painter.setPen(conf.inactiveTextShadowColor());
        }
        int dx = conf.textShadowOffsetX();
        int dy = conf.textShadowOffsetY();
        painter.setOpacity(0.5);
        painter.drawText(pixmap.rect().translated(dx, dy),
                            conf.alignment() | conf.verticalAlignment() | Qt::TextSingleLine,
                            caption());
        painter.setOpacity(0.2);
        painter.drawText(pixmap.rect().translated(dx+1, dy),
                            conf.alignment() | conf.verticalAlignment() | Qt::TextSingleLine,
                            caption());
        painter.drawText(pixmap.rect().translated(dx-1, dy),
                            conf.alignment() | conf.verticalAlignment() | Qt::TextSingleLine,
                            caption());
        painter.drawText(pixmap.rect().translated(dx, dy+1),
                            conf.alignment() | conf.verticalAlignment() | Qt::TextSingleLine,
                            caption());
        painter.drawText(pixmap.rect().translated(dx, dy-1),
                            conf.alignment() | conf.verticalAlignment() | Qt::TextSingleLine,
                            caption());
        painter.restore();
    }
    if (active) {
        painter.setPen(conf.activeTextColor());
    } else {
        painter.setPen(conf.inactiveTextColor());
    }
    painter.drawText(pixmap.rect(), conf.alignment() | conf.verticalAlignment() | Qt::TextSingleLine,
                      caption());
    painter.end();
}

void AuroraeClient::captionChange()
{
    generateTextPixmap(m_activeText, true);
    generateTextPixmap(m_inactiveText, false);
    KCommonDecoration::captionChange();
}

void AuroraeClient::updateWindowShape()
{
    bool maximized = maximizeMode()==KDecorationDefines::MaximizeFull && !options()->moveResizeMaximizedWindows();
    int w=widget()->width();
    int h=widget()->height();

    if (maximized || compositingActive()) {
        QRegion mask(0,0,w,h);
        setMask(mask);
        return;
    }

    const ThemeConfig &conf = AuroraeFactory::instance()->themeConfig();
    Plasma::FrameSvg *deco = AuroraeFactory::instance()->frame();
    if (!deco->hasElementPrefix("decoration-opaque")) {
        // opaque element is missing: set generic mask
        w = w - conf.paddingLeft() - conf.paddingRight();
        h = h - conf.paddingTop() - conf.paddingBottom();
        QRegion mask(conf.paddingLeft(),conf.paddingTop(),w,h);
        setMask(mask);
        return;
    }
    deco->setElementPrefix("decoration-opaque");
    deco->resizeFrame(QSize(w-conf.paddingLeft()-conf.paddingRight(),
                                h-conf.paddingTop()-conf.paddingBottom()));
    QRegion mask = deco->mask().translated(conf.paddingLeft(), conf.paddingTop());
    setMask(mask);
}

void AuroraeClient::activeChange()
{
    if (m_animationId != 0) {
        Plasma::Animator::self()->stopCustomAnimation(m_animationId);
    }
    m_animationProgress = 0.0;
    int time = AuroraeFactory::instance()->themeConfig().animationTime();
    if (time != 0) {
        m_animationId = Plasma::Animator::self()->customAnimation(40 / (1000.0 / qreal(time)),
                                                                  time, Plasma::Animator::EaseInOutCurve, this, "animationUpdate");
    }
    KCommonDecoration::activeChange();
}

void AuroraeClient::animationUpdate(qreal progress, int id)
{
    if (m_animationId == id) {
        m_animationProgress = progress;
        widget()->update();
    }
}

void AuroraeClient::animationFinished(int id)
{
    if (m_animationId == id) {
        m_animationId = 0;
        widget()->update();
    }
}

void AuroraeClient::resize(const QSize& s)
{
    KCommonDecoration::resize(s);
    m_activeText = QPixmap(titleRect().size());
    m_activeText.fill(Qt::transparent);
    m_inactiveText = QPixmap(titleRect().size());
    m_inactiveText.fill(Qt::transparent);
    captionChange();
}

} // namespace Aurorae

extern "C"
{
    KDE_EXPORT KDecorationFactory *create_factory() {
        return Aurorae::AuroraeFactory::instance();
    }
}


#include "aurorae.moc"
