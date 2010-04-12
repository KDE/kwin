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
};

AuroraeThemePrivate::AuroraeThemePrivate()
    : decoration(0)
    , activeCompositing(true)
    , borderSize(KDecoration::BorderNormal)
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
        d->themeName = QString();
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
    default:
        return QLatin1String("");
    }
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
    if (maximized) {
        left   = 0;
        right  = 0;
        bottom = 0;
        top    = d->themeConfig.titleHeight() + d->themeConfig.titleEdgeTopMaximized() + d->themeConfig.titleEdgeBottomMaximized();
    } else {
        switch (d->borderSize) {
        case KDecoration::BorderTiny:
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
            left = right = bottom = 4;
            break;
        case KDecoration::BorderVeryLarge:
            left = right = bottom = 8;
            break;
        case KDecoration::BorderHuge:
            left = right = bottom = 12;
            break;
        case KDecoration::BorderVeryHuge:
            left = right = bottom = 23;
            break;
        case KDecoration::BorderOversized:
            left = right = bottom = 36;
            break;
        case KDecoration::BorderNormal:
        default:
            left = right = bottom =  0;
        }
        left   += d->themeConfig.borderLeft();
        right  += d->themeConfig.borderRight();
        bottom += d->themeConfig.borderBottom();
        top     = d->themeConfig.titleHeight() + d->themeConfig.titleEdgeTop() + d->themeConfig.titleEdgeBottom();
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


} // namespace
