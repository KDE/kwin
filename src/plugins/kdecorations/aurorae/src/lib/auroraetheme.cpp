/*
    Library for Aurorae window decoration themes.
    SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later

*/

#include "auroraetheme.h"
#include "themeconfig.h"
// Qt
#include <QDebug>
#include <QHash>
#include <QStandardPaths>
// KDE
#include <KConfig>
#include <KConfigGroup>

Q_LOGGING_CATEGORY(AURORAE, "aurorae", QtWarningMsg)

namespace Aurorae
{

/************************************************
 * AuroraeThemePrivate
 ************************************************/
class AuroraeThemePrivate
{
public:
    AuroraeThemePrivate();
    ~AuroraeThemePrivate();
    void initButtonFrame(AuroraeButtonType type);
    QString themeName;
    Aurorae::ThemeConfig themeConfig;
    QHash<AuroraeButtonType, QString> pathes;
    bool activeCompositing;
    KDecoration2::BorderSize borderSize;
    KDecoration2::BorderSize buttonSize;
    QString dragMimeType;
    QString decorationPath;
};

AuroraeThemePrivate::AuroraeThemePrivate()
    : activeCompositing(true)
    , borderSize(KDecoration2::BorderSize::Normal)
    , buttonSize(KDecoration2::BorderSize::Normal)
{
}

AuroraeThemePrivate::~AuroraeThemePrivate()
{
}

void AuroraeThemePrivate::initButtonFrame(AuroraeButtonType type)
{
    QString file(QLatin1String("aurorae/themes/") + themeName + QLatin1Char('/') + AuroraeTheme::mapButtonToName(type) + QLatin1String(".svg"));
    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, file);
    if (path.isEmpty()) {
        // let's look for svgz
        file += QLatin1String("z");
        path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, file);
    }
    if (!path.isEmpty()) {
        pathes[type] = path;
    } else {
        qCDebug(AURORAE) << "No button for: " << AuroraeTheme::mapButtonToName(type);
    }
}

/************************************************
 * AuroraeTheme
 ************************************************/
AuroraeTheme::AuroraeTheme(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<AuroraeThemePrivate>())
{
    connect(this, &AuroraeTheme::themeChanged, this, &AuroraeTheme::borderSizesChanged);
    connect(this, &AuroraeTheme::buttonSizesChanged, this, &AuroraeTheme::borderSizesChanged);
}

AuroraeTheme::~AuroraeTheme() = default;

bool AuroraeTheme::isValid() const
{
    return !d->themeName.isNull();
}

void AuroraeTheme::loadTheme(const QString &name)
{
    KConfig conf(QStringLiteral("auroraerc"));
    KConfig config(QLatin1String("aurorae/themes/") + name + QLatin1Char('/') + name + QLatin1String("rc"),
                   KConfig::FullConfig, QStandardPaths::GenericDataLocation);
    KConfigGroup themeGroup(&conf, name);
    loadTheme(name, config);
}

void AuroraeTheme::loadTheme(const QString &name, const KConfig &config)
{
    d->themeName = name;
    QString file(QLatin1String("aurorae/themes/") + d->themeName + QLatin1String("/decoration.svg"));
    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, file);
    if (path.isEmpty()) {
        file += QLatin1String("z");
        path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, file);
    }
    if (path.isEmpty()) {
        qCDebug(AURORAE) << "Could not find decoration svg: aborting";
        d->themeName.clear();
        return;
    }
    d->decorationPath = path;

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
    Q_EMIT themeChanged();
}

bool AuroraeTheme::hasButton(AuroraeButtonType button) const
{
    return d->pathes.contains(button);
}

QLatin1String AuroraeTheme::mapButtonToName(AuroraeButtonType type)
{
    switch (type) {
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
    case AppMenuButton:
        return QLatin1String("appmenu");
    default:
        return QLatin1String("");
    }
}

const QString &AuroraeTheme::themeName() const
{
    return d->themeName;
}

void AuroraeTheme::borders(int &left, int &top, int &right, int &bottom, bool maximized) const
{
    const qreal titleHeight = std::max((qreal)d->themeConfig.titleHeight(),
                                       d->themeConfig.buttonHeight() * buttonSizeFactor() + d->themeConfig.buttonMarginTop());
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
        int minMargin;
        int maxMargin;
        switch (d->borderSize) {
        case KDecoration2::BorderSize::NoSides:
        case KDecoration2::BorderSize::Tiny:
            minMargin = 1;
            maxMargin = 4;
            break;
        case KDecoration2::BorderSize::Normal:
            minMargin = 4;
            maxMargin = 6;
            break;
        case KDecoration2::BorderSize::Large:
            minMargin = 6;
            maxMargin = 8;
            break;
        case KDecoration2::BorderSize::VeryLarge:
            minMargin = 8;
            maxMargin = 12;
            break;
        case KDecoration2::BorderSize::Huge:
            minMargin = 12;
            maxMargin = 20;
            break;
        case KDecoration2::BorderSize::VeryHuge:
            minMargin = 23;
            maxMargin = 30;
            break;
        case KDecoration2::BorderSize::Oversized:
            minMargin = 36;
            maxMargin = 48;
            break;
        default:
            minMargin = 0;
            maxMargin = 0;
        }

        left = std::clamp(d->themeConfig.borderLeft(), minMargin, maxMargin);
        right = std::clamp(d->themeConfig.borderRight(), minMargin, maxMargin);
        bottom = std::clamp(d->themeConfig.borderBottom(), minMargin, maxMargin);

        if (d->borderSize == KDecoration2::BorderSize::None) {
            left = 0;
            right = 0;
            bottom = 0;
        } else if (d->borderSize == KDecoration2::BorderSize::NoSides) {
            left = 0;
            right = 0;
        }

        const qreal title = titleHeight + d->themeConfig.titleEdgeTop() + d->themeConfig.titleEdgeBottom();
        switch ((DecorationPosition)d->themeConfig.decorationPosition()) {
        case DecorationTop:
            top = title;
            break;
        case DecorationBottom:
            bottom = title;
            break;
        case DecorationLeft:
            left = title;
            break;
        case DecorationRight:
            right = title;
            break;
        default:
            left = right = bottom = top = 0;
            break;
        }
    }
}

int AuroraeTheme::bottomBorder() const
{
    int left, top, right, bottom;
    left = top = right = bottom = 0;
    borders(left, top, right, bottom, false);
    return bottom;
}

int AuroraeTheme::leftBorder() const
{
    int left, top, right, bottom;
    left = top = right = bottom = 0;
    borders(left, top, right, bottom, false);
    return left;
}

int AuroraeTheme::rightBorder() const
{
    int left, top, right, bottom;
    left = top = right = bottom = 0;
    borders(left, top, right, bottom, false);
    return right;
}

int AuroraeTheme::topBorder() const
{
    int left, top, right, bottom;
    left = top = right = bottom = 0;
    borders(left, top, right, bottom, false);
    return top;
}

int AuroraeTheme::bottomBorderMaximized() const
{
    int left, top, right, bottom;
    left = top = right = bottom = 0;
    borders(left, top, right, bottom, true);
    return bottom;
}

int AuroraeTheme::leftBorderMaximized() const
{
    int left, top, right, bottom;
    left = top = right = bottom = 0;
    borders(left, top, right, bottom, true);
    return left;
}

int AuroraeTheme::rightBorderMaximized() const
{
    int left, top, right, bottom;
    left = top = right = bottom = 0;
    borders(left, top, right, bottom, true);
    return right;
}

int AuroraeTheme::topBorderMaximized() const
{
    int left, top, right, bottom;
    left = top = right = bottom = 0;
    borders(left, top, right, bottom, true);
    return top;
}

void AuroraeTheme::padding(int &left, int &top, int &right, int &bottom) const
{
    left = d->themeConfig.paddingLeft();
    top = d->themeConfig.paddingTop();
    right = d->themeConfig.paddingRight();
    bottom = d->themeConfig.paddingBottom();
}

#define THEME_CONFIG(prototype)            \
    int AuroraeTheme::prototype() const    \
    {                                      \
        return d->themeConfig.prototype(); \
    }

THEME_CONFIG(paddingBottom)
THEME_CONFIG(paddingLeft)
THEME_CONFIG(paddingRight)
THEME_CONFIG(paddingTop)
THEME_CONFIG(buttonWidth)
THEME_CONFIG(buttonWidthMinimize)
THEME_CONFIG(buttonWidthMaximizeRestore)
THEME_CONFIG(buttonWidthClose)
THEME_CONFIG(buttonWidthAllDesktops)
THEME_CONFIG(buttonWidthKeepAbove)
THEME_CONFIG(buttonWidthKeepBelow)
THEME_CONFIG(buttonWidthShade)
THEME_CONFIG(buttonWidthHelp)
THEME_CONFIG(buttonWidthMenu)
THEME_CONFIG(buttonWidthAppMenu)
THEME_CONFIG(buttonHeight)
THEME_CONFIG(buttonSpacing)
THEME_CONFIG(buttonMarginTop)
THEME_CONFIG(explicitButtonSpacer)
THEME_CONFIG(animationTime)
THEME_CONFIG(titleEdgeLeft)
THEME_CONFIG(titleEdgeRight)
THEME_CONFIG(titleEdgeTop)
THEME_CONFIG(titleEdgeLeftMaximized)
THEME_CONFIG(titleEdgeRightMaximized)
THEME_CONFIG(titleEdgeTopMaximized)
THEME_CONFIG(titleBorderLeft)
THEME_CONFIG(titleBorderRight)
THEME_CONFIG(titleHeight)

#undef THEME_CONFIG

#define THEME_CONFIG_TYPE(rettype, prototype) \
    rettype AuroraeTheme::prototype() const   \
    {                                         \
        return d->themeConfig.prototype();    \
    }

THEME_CONFIG_TYPE(QColor, activeTextColor)
THEME_CONFIG_TYPE(QColor, inactiveTextColor)
THEME_CONFIG_TYPE(Qt::Alignment, alignment)
THEME_CONFIG_TYPE(Qt::Alignment, verticalAlignment)

#undef THEME_CONFIG_TYPE

QString AuroraeTheme::decorationPath() const
{
    return d->decorationPath;
}

#define BUTTON_PATH(prototype, buttonType)  \
    QString AuroraeTheme::prototype() const \
    {                                       \
        if (hasButton(buttonType)) {        \
            return d->pathes[buttonType];   \
        } else {                            \
            return QString();               \
        }                                   \
    }

BUTTON_PATH(minimizeButtonPath, MinimizeButton)
BUTTON_PATH(maximizeButtonPath, MaximizeButton)
BUTTON_PATH(restoreButtonPath, RestoreButton)
BUTTON_PATH(closeButtonPath, CloseButton)
BUTTON_PATH(allDesktopsButtonPath, AllDesktopsButton)
BUTTON_PATH(keepAboveButtonPath, KeepAboveButton)
BUTTON_PATH(keepBelowButtonPath, KeepBelowButton)
BUTTON_PATH(shadeButtonPath, ShadeButton)
BUTTON_PATH(helpButtonPath, HelpButton)

#undef BUTTON_PATH

void AuroraeTheme::titleEdges(int &left, int &top, int &right, int &bottom, bool maximized) const
{
    if (maximized) {
        left = d->themeConfig.titleEdgeLeftMaximized();
        top = d->themeConfig.titleEdgeTopMaximized();
        right = d->themeConfig.titleEdgeRightMaximized();
        bottom = d->themeConfig.titleEdgeBottomMaximized();
    } else {
        left = d->themeConfig.titleEdgeLeft();
        top = d->themeConfig.titleEdgeTop();
        right = d->themeConfig.titleEdgeRight();
        bottom = d->themeConfig.titleEdgeBottom();
    }
}

bool AuroraeTheme::isCompositingActive() const
{
    return d->activeCompositing;
}

void AuroraeTheme::setCompositingActive(bool active)
{
    d->activeCompositing = active;
}

void AuroraeTheme::setBorderSize(KDecoration2::BorderSize size)
{
    if (d->borderSize == size) {
        return;
    }
    d->borderSize = size;
    Q_EMIT borderSizesChanged();
}

void AuroraeTheme::setButtonSize(KDecoration2::BorderSize size)
{
    if (d->buttonSize == size) {
        return;
    }
    d->buttonSize = size;
    Q_EMIT buttonSizesChanged();
}

void AuroraeTheme::setTabDragMimeType(const QString &mime)
{
    d->dragMimeType = mime;
}

const QString &AuroraeTheme::tabDragMimeType() const
{
    return d->dragMimeType;
}

qreal AuroraeTheme::buttonSizeFactor() const
{
    switch (d->buttonSize) {
    case KDecoration2::BorderSize::Tiny:
        return 0.8;
    case KDecoration2::BorderSize::Large:
        return 1.2;
    case KDecoration2::BorderSize::VeryLarge:
        return 1.4;
    case KDecoration2::BorderSize::Huge:
        return 1.6;
    case KDecoration2::BorderSize::VeryHuge:
        return 1.8;
    case KDecoration2::BorderSize::Oversized:
        return 2.0;
    case KDecoration2::BorderSize::Normal: // fall through
    default:
        return 1.0;
    }
}

DecorationPosition AuroraeTheme::decorationPosition() const
{
    return (DecorationPosition)d->themeConfig.decorationPosition();
}

} // namespace
