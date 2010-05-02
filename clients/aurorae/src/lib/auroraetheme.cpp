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

#include "auroraetheme.h"
#include "themeconfig.h"
// Qt
#include <QtCore/QHash>
// KDE
#include <KDE/KConfig>
#include <KDE/KConfigGroup>
#include <KDE/KDebug>
#include <KDE/KStandardDirs>
#include <KDE/KGlobal>
#include <KDE/Plasma/FrameSvg>

namespace Aurorae {

/************************************************
* AuroraeThemePrivate
************************************************/
class AuroraeThemePrivate
{
public:
    AuroraeThemePrivate();
    ~AuroraeThemePrivate();
    void initButtonFrame(AuroraeButtonType type);
    void reset();
    QString themeName;
    Aurorae::ThemeConfig themeConfig;
    Plasma::FrameSvg *decoration;
    QHash< AuroraeButtonType, Plasma::FrameSvg* > buttons;
    bool activeCompositing;
    KDecorationDefines::BorderSize borderSize;
    bool showTooltips;
    KDecorationDefines::BorderSize buttonSize;
};

AuroraeThemePrivate::AuroraeThemePrivate()
    : decoration(0)
    , activeCompositing(true)
    , borderSize(KDecoration::BorderNormal)
    , showTooltips(true)
    , buttonSize(KDecoration::BorderNormal)
{
}

AuroraeThemePrivate::~AuroraeThemePrivate()
{
    while (!buttons.isEmpty()) {
        Plasma::FrameSvg *button = buttons.begin().value();
        delete button;
        button = 0;
        buttons.remove(buttons.begin().key());
    }
}

void AuroraeThemePrivate::initButtonFrame(AuroraeButtonType type)
{
    QString file("aurorae/themes/" + themeName + '/' + AuroraeTheme::mapButtonToName(type) + ".svg");
    QString path = KGlobal::dirs()->findResource("data", file);
    if (path.isEmpty()) {
        // let's look for svgz
        file.append("z");
        path = KGlobal::dirs()->findResource("data", file);
    }
    if (!path.isEmpty()) {
        Plasma::FrameSvg *frame = new Plasma::FrameSvg();
        frame->setImagePath(path);
        frame->setCacheAllRenderedFrames(true);
        frame->setEnabledBorders(Plasma::FrameSvg::NoBorder);
        buttons[ type ] = frame;
    } else {
        kDebug(1216) << "No button for: " << AuroraeTheme::mapButtonToName(type);
    }
}

void AuroraeThemePrivate::reset()
{
    decoration->clearCache();
    delete decoration;
    decoration = 0;
    while (!buttons.isEmpty()) {
        Plasma::FrameSvg *button = buttons.begin().value();
        button->clearCache();
        delete button;
        button = 0;
        buttons.remove(buttons.begin().key());
    }
}

/************************************************
* AuroraeTheme
************************************************/
AuroraeTheme::AuroraeTheme(QObject* parent)
    : QObject(parent)
    , d(new AuroraeThemePrivate)
{
}

AuroraeTheme::~AuroraeTheme()
{
    delete d;
}

bool AuroraeTheme::isValid() const
{
    return !d->themeName.isNull();
}

void AuroraeTheme::loadTheme(const QString &name, const KConfig &config)
{
    if (!d->themeName.isNull()) {
        // only reset if the theme has been initialized at least once
        d->reset();
    }
    d->themeName = name;
    QString file("aurorae/themes/" + d->themeName + "/decoration.svg");
    QString path = KGlobal::dirs()->findResource("data", file);
    if (path.isEmpty()) {
        file += 'z';
        path = KGlobal::dirs()->findResource("data", file);
    }
    if (path.isEmpty()) {
        kDebug(1216) << "Could not find decoration svg: aborting";
        d->themeName.clear();
        return;
    }
    d->decoration = new Plasma::FrameSvg(this);
    d->decoration->setImagePath(path);
    d->decoration->setCacheAllRenderedFrames(true);
    d->decoration->setEnabledBorders(Plasma::FrameSvg::AllBorders);

    // load the buttons
    d->initButtonFrame(MinimizeButton);
    d->initButtonFrame(MaximizeButton);
    d->initButtonFrame(RestoreButton);
    d->initButtonFrame(CloseButton);
    d->initButtonFrame(AllDesktopsButton);
    d->initButtonFrame(KeepAboveButton);
    d->initButtonFrame(KeepBelowButton);
    d->initButtonFrame(ShadeButton);
    d->initButtonFrame(HelpButton);

    d->themeConfig.load(config);
    emit themeChanged();
}

void AuroraeTheme::readThemeConfig(const KConfig &config)
{
    // read config values
    d->themeConfig.load(config);
    emit themeChanged();
}

Plasma::FrameSvg *AuroraeTheme::button(AuroraeButtonType b) const
{
    if (hasButton(b)) {
        return d->buttons[ b ];
    } else {
        return NULL;
    }
}

Plasma::FrameSvg *AuroraeTheme::decoration() const
{
    return d->decoration;
}

bool AuroraeTheme::hasButton(AuroraeButtonType button) const
{
    return d->buttons.contains(button);
}

QLatin1String AuroraeTheme::mapButtonToName(AuroraeButtonType type)
{
    switch(type) {
    case MinimizeButton:
        return QLatin1String("minimize");
    case MaximizeButton:
        return QLatin1String("maximize");
    case RestoreButton:
        return QLatin1String("restore");
    case CloseButton:
        return QLatin1String("close");
    case AllDesktopsButton:
        return QLatin1String("alldesktops");
    case KeepAboveButton:
        return QLatin1String("keepabove");
    case KeepBelowButton:
        return QLatin1String("keepbelow");
    case ShadeButton:
        return QLatin1String("shade");
    case HelpButton:
        return QLatin1String("help");
    case MenuButton:
        return QLatin1String("menu");
    default:
        return QLatin1String("");
    }
}

char AuroraeTheme::mapButtonToChar(AuroraeButtonType type)
{
    char c = ' ';
    switch (type) {
    case Aurorae::MinimizeButton:
        c = 'I';
        break;
    case Aurorae::MaximizeButton: // fall through
    case Aurorae::RestoreButton:
        c = 'A';
        break;
    case Aurorae::CloseButton:
        c = 'X';
        break;
    case Aurorae::AllDesktopsButton:
        c = 'S';
        break;
    case Aurorae::KeepAboveButton:
        c = 'F';
        break;
    case Aurorae::KeepBelowButton:
        c = 'B';
        break;
    case Aurorae::ShadeButton:
        c = 'L';
        break;
    case Aurorae::HelpButton:
        c = 'H';
        break;
    case Aurorae::MenuButton:
        c = 'M';
        break;
    default:
        break; // nothing
    }
    return c;
}

const QString &AuroraeTheme::themeName() const
{
    return d->themeName;
}
const Aurorae::ThemeConfig &AuroraeTheme::themeConfig() const
{
    return d->themeConfig;
}

void AuroraeTheme::borders(int& left, int& top, int& right, int& bottom, bool maximized) const
{
    const qreal titleHeight = qMax((qreal)d->themeConfig.titleHeight(),
                                   d->themeConfig.buttonHeight()*buttonSizeFactor() +
                                   d->themeConfig.buttonMarginTop());
    if (maximized) {
        const qreal title = titleHeight + d->themeConfig.titleEdgeTopMaximized() + d->themeConfig.titleEdgeBottomMaximized();
        switch ((DecorationPosition)d->themeConfig.decorationPosition()) {
        case DecorationTop:
            left = right = bottom = 0;
            top = title;
            break;
        case DecorationBottom:
            left = right = top = 0;
            bottom = title;
            break;
        case DecorationLeft:
            top = right = bottom = 0;
            left = title;
            break;
        case DecorationRight:
            left = top = bottom = 0;
            right = title;
            break;
        default:
            left = right = bottom = top = 0;
            break;
        }
    } else {
        switch (d->borderSize) {
        case KDecoration::BorderTiny:
            // TODO: this looks wrong
            if (isCompositingActive()) {
                left = qMin(0, (int)left - d->themeConfig.borderLeft() - d->themeConfig.paddingLeft());
                right = qMin(0, (int)right - d->themeConfig.borderRight() - d->themeConfig.paddingRight());
                bottom = qMin(0, (int)bottom - d->themeConfig.borderBottom() - d->themeConfig.paddingBottom());
            } else {
                left = qMin(0, (int)left - d->themeConfig.borderLeft());
                right = qMin(0, (int)right - d->themeConfig.borderRight());
                bottom = qMin(0, (int)bottom - d->themeConfig.borderBottom());
            }
            break;
        case KDecoration::BorderLarge:
            left = right = bottom = top = 4;
            break;
        case KDecoration::BorderVeryLarge:
            left = right = bottom = top = 8;
            break;
        case KDecoration::BorderHuge:
            left = right = bottom = top = 12;
            break;
        case KDecoration::BorderVeryHuge:
            left = right = bottom = top = 23;
            break;
        case KDecoration::BorderOversized:
            left = right = bottom = top = 36;
            break;
        case KDecoration::BorderNormal:
        default:
            left = right = bottom = top = 0;
        }
        const qreal title = titleHeight + d->themeConfig.titleEdgeTop() + d->themeConfig.titleEdgeBottom();
        switch ((DecorationPosition)d->themeConfig.decorationPosition()) {
        case DecorationTop:
            left   += d->themeConfig.borderLeft();
            right  += d->themeConfig.borderRight();
            bottom += d->themeConfig.borderBottom();
            top     = title;
            break;
        case DecorationBottom:
            left   += d->themeConfig.borderLeft();
            right  += d->themeConfig.borderRight();
            bottom  = title;
            top    += d->themeConfig.borderTop();
            break;
        case DecorationLeft:
            left    = title;
            right  += d->themeConfig.borderRight();
            bottom += d->themeConfig.borderBottom();
            top    += d->themeConfig.borderTop();
            break;
        case DecorationRight:
            left   += d->themeConfig.borderLeft();
            right   = title;
            bottom += d->themeConfig.borderBottom();
            top    += d->themeConfig.borderTop();
            break;
        default:
            left = right = bottom = top = 0;
            break;
        }
    }
}

void AuroraeTheme::padding(int& left, int& top, int& right, int& bottom) const
{
    left   = d->themeConfig.paddingLeft();
    top    = d->themeConfig.paddingTop();
    right  = d->themeConfig.paddingRight();
    bottom = d->themeConfig.paddingBottom();
}

void AuroraeTheme::titleEdges(int &left, int &top, int &right, int &bottom, bool maximized) const
{
    if (maximized) {
        left   = d->themeConfig.titleEdgeLeftMaximized();
        top    = d->themeConfig.titleEdgeTopMaximized();
        right  = d->themeConfig.titleEdgeRightMaximized();
        bottom = d->themeConfig.titleEdgeBottomMaximized();
    } else {
        left   = d->themeConfig.titleEdgeLeft();
        top    = d->themeConfig.titleEdgeTop();
        right  = d->themeConfig.titleEdgeRight();
        bottom = d->themeConfig.titleEdgeBottom();
    }
}

void AuroraeTheme::buttonMargins(int &left, int &top, int &right) const
{
    left   = d->themeConfig.titleBorderLeft();
    top    = d->themeConfig.buttonMarginTop();
    right  = d->themeConfig.titleBorderRight();
}

bool AuroraeTheme::isCompositingActive() const
{
    return d->activeCompositing;
}

void AuroraeTheme::setCompositingActive(bool active)
{
    d->activeCompositing = active;
}

QString AuroraeTheme::defaultButtonsLeft() const
{
    return d->themeConfig.defaultButtonsLeft();
}

QString AuroraeTheme::defaultButtonsRight() const
{
    return d->themeConfig.defaultButtonsRight();
}

void AuroraeTheme::setBorderSize(KDecorationDefines::BorderSize size)
{
    d->borderSize = size;
}

bool AuroraeTheme::isShowTooltips() const
{
    return d->showTooltips;
}

void AuroraeTheme::setShowTooltips(bool show)
{
    d->showTooltips = show;
    emit showTooltipsChanged(show);
}

void AuroraeTheme::setButtonSize(KDecorationDefines::BorderSize size)
{
    d->buttonSize = size;
    emit buttonSizesChanged();
}

qreal AuroraeTheme::buttonSizeFactor() const
{
    switch (d->buttonSize) {
    case KDecorationDefines::BorderTiny:
        return 0.8;
    case KDecorationDefines::BorderLarge:
        return 1.2;
    case KDecorationDefines::BorderVeryLarge:
        return 1.4;
    case KDecorationDefines::BorderHuge:
        return 1.6;
    case KDecorationDefines::BorderVeryHuge:
        return 1.8;
    case KDecorationDefines::BorderOversized:
        return 2.0;
    case KDecorationDefines::BorderNormal: // fall through
    default:
        return 1.0;
    }
}

DecorationPosition AuroraeTheme::decorationPosition() const
{
    return (DecorationPosition)d->themeConfig.decorationPosition();
}

} // namespace
