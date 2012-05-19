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
#include "themeconfig.h"
#include <kdecoration.h>

#include <KConfig>
#include <KConfigGroup>

namespace Aurorae
{

ThemeConfig::ThemeConfig()
    : m_activeTextColor(defaultActiveTextColor())
    , m_activeFocusedTextColor(defaultActiveFocusedTextColor())
    , m_activeUnfocusedTextColor(defaultActiveUnfocusedTextColor())
    , m_inactiveTextColor(defaultInactiveTextColor())
    , m_inactiveFocusedTextColor(defaultInactiveFocusedTextColor())
    , m_inactiveUnfocusedTextColor(defaultInactiveUnfocusedTextColor())
    , m_activeTextShadowColor(defaultActiveTextShadowColor())
    , m_inactiveTextShadowColor(defaultInactiveTextShadowColor())
    , m_textShadowOffsetX(defaultTextShadowOffsetX())
    , m_textShadowOffsetY(defaultTextShadowOffsetY())
    , m_useTextShadow(defaultUseTextShadow())
    , m_haloActive(defaultHaloActive())
    , m_haloInactive(defaultHaloInactive())
    , m_alignment(defaultAlignment())
    , m_verticalAlignment(defaultVerticalAlignment())
    // borders
    , m_borderLeft(defaultBorderLeft())
    , m_borderRight(defaultBorderRight())
    , m_borderBottom(defaultBorderBottom())
    , m_borderTop(defaultBorderTop())
    // title
    , m_titleEdgeTop(defaultTitleEdgeTop())
    , m_titleEdgeBottom(defaultTitleEdgeBottom())
    , m_titleEdgeLeft(defaultTitleEdgeLeft())
    , m_titleEdgeRight(defaultTitleEdgeRight())
    , m_titleEdgeTopMaximized(defaultTitleEdgeTopMaximized())
    , m_titleEdgeBottomMaximized(defaultTitleEdgeBottomMaximized())
    , m_titleEdgeLeftMaximized(defaultTitleEdgeLeftMaximized())
    , m_titleEdgeRightMaximized(defaultTitleEdgeRightMaximized())
    , m_titleBorderLeft(defaultTitleBorderLeft())
    , m_titleBorderRight(defaultTitleBorderRight())
    , m_titleHeight(defaultTitleHeight())
    // buttons
    , m_buttonWidth(defaultButtonWidth())
    , m_buttonWidthMinimize(defaultButtonWidthMinimize())
    , m_buttonWidthMaximizeRestore(defaultButtonWidthMaximizeRestore())
    , m_buttonWidthClose(defaultButtonWidthClose())
    , m_buttonWidthAllDesktops(defaultButtonWidthAllDesktops())
    , m_buttonWidthKeepAbove(defaultButtonWidthKeepAbove())
    , m_buttonWidthKeepBelow(defaultButtonWidthKeepBelow())
    , m_buttonWidthShade(defaultButtonWidthShade())
    , m_buttonWidthHelp(defaultButtonWidthHelp())
    , m_buttonWidthMenu(defaultButtonWidthMenu())
    , m_buttonHeight(defaultButtonHeight())
    , m_buttonSpacing(defaultButtonSpacing())
    , m_buttonMarginTop(defaultButtonMarginTop())
    , m_explicitButtonSpacer(defaultExplicitButtonSpacer())
    // padding
    , m_paddingLeft(defaultPaddingLeft())
    , m_paddingRight(defaultPaddingRight())
    , m_paddingTop(defaultPaddingTop())
    , m_paddingBottom(defaultPaddingBottom())
    , m_animationTime(defaultAnimationTime())
    , m_defaultButtonsLeft(KDecorationOptions::defaultTitleButtonsLeft())
    , m_defaultButtonsRight(KDecorationOptions::defaultTitleButtonsRight())
    , m_shadow(defaultShadow())
    , m_decorationPosition(defaultDecorationPosition())
{
}

void ThemeConfig::load(const KConfig &conf)
{
    KConfigGroup general(&conf, "General");
    m_activeTextColor = general.readEntry("ActiveTextColor", defaultActiveTextColor());
    m_inactiveTextColor = general.readEntry("InactiveTextColor", defaultInactiveTextColor());
    m_activeFocusedTextColor = general.readEntry("ActiveFocusedTabColor", m_activeTextColor);
    m_activeUnfocusedTextColor = general.readEntry("ActiveUnfocusedTabColor", m_inactiveTextColor);
    m_inactiveFocusedTextColor = general.readEntry("InactiveFocusedTabColor", m_inactiveTextColor);
    m_inactiveUnfocusedTextColor = general.readEntry("InactiveUnfocusedTabColor", m_inactiveTextColor);
    m_useTextShadow = general.readEntry("UseTextShadow", defaultUseTextShadow());
    m_activeTextShadowColor = general.readEntry("ActiveTextShadowColor", defaultActiveTextColor());
    m_inactiveTextShadowColor = general.readEntry("InactiveTextShadowColor", defaultInactiveTextColor());
    m_textShadowOffsetX = general.readEntry("TextShadowOffsetX", defaultTextShadowOffsetX());
    m_textShadowOffsetY = general.readEntry("TextShadowOffsetY", defaultTextShadowOffsetY());
    m_haloActive = general.readEntry("HaloActive", defaultHaloActive());
    m_haloInactive = general.readEntry("HaloInactive", defaultHaloInactive());
    QString alignment = (general.readEntry("TitleAlignment", "Left")).toLower();
    if (alignment == "left") {
        m_alignment = Qt::AlignLeft;
    }
    else if (alignment == "center") {
        m_alignment = Qt::AlignCenter;
    }
    else {
        m_alignment = Qt::AlignRight;
    }
    alignment = (general.readEntry("TitleVerticalAlignment", "Center")).toLower();
    if (alignment == "top") {
        m_verticalAlignment = Qt::AlignTop;
    }
    else if (alignment == "center") {
        m_verticalAlignment = Qt::AlignVCenter;
    }
    else {
        m_verticalAlignment = Qt::AlignBottom;
    }
    m_animationTime = general.readEntry("Animation", defaultAnimationTime());
    m_defaultButtonsLeft = general.readEntry("LeftButtons", KDecorationOptions::defaultTitleButtonsLeft());
    m_defaultButtonsRight = general.readEntry("RightButtons", KDecorationOptions::defaultTitleButtonsRight());
    m_shadow = general.readEntry("Shadow", defaultShadow());
    m_decorationPosition = general.readEntry("DecorationPosition", defaultDecorationPosition());

    KConfigGroup border(&conf, "Layout");
    // default values taken from KCommonDecoration::layoutMetric() in kcommondecoration.cpp
    m_borderLeft = border.readEntry("BorderLeft", defaultBorderLeft());
    m_borderRight = border.readEntry("BorderRight", defaultBorderRight());
    m_borderBottom = border.readEntry("BorderBottom", defaultBorderBottom());
    m_borderTop = border.readEntry("BorderTop", defaultBorderTop());

    m_titleEdgeTop = border.readEntry("TitleEdgeTop", defaultTitleEdgeTop());
    m_titleEdgeBottom = border.readEntry("TitleEdgeBottom", defaultTitleEdgeBottom());
    m_titleEdgeLeft = border.readEntry("TitleEdgeLeft", defaultTitleEdgeLeft());
    m_titleEdgeRight = border.readEntry("TitleEdgeRight", defaultTitleEdgeRight());
    m_titleEdgeTopMaximized = border.readEntry("TitleEdgeTopMaximized", defaultTitleEdgeTopMaximized());
    m_titleEdgeBottomMaximized = border.readEntry("TitleEdgeBottomMaximized", defaultTitleEdgeBottomMaximized());
    m_titleEdgeLeftMaximized = border.readEntry("TitleEdgeLeftMaximized", defaultTitleEdgeLeftMaximized());
    m_titleEdgeRightMaximized = border.readEntry("TitleEdgeRightMaximized", defaultTitleEdgeRightMaximized());
    m_titleBorderLeft = border.readEntry("TitleBorderLeft", defaultTitleBorderLeft());
    m_titleBorderRight = border.readEntry("TitleBorderRight", defaultTitleBorderRight());
    m_titleHeight = border.readEntry("TitleHeight", defaultTitleHeight());

    m_buttonWidth = border.readEntry("ButtonWidth", defaultButtonWidth());
    m_buttonWidthMinimize = border.readEntry("ButtonWidthMinimize", m_buttonWidth);
    m_buttonWidthMaximizeRestore = border.readEntry("ButtonWidthMaximizeRestore", m_buttonWidth);
    m_buttonWidthClose = border.readEntry("ButtonWidthClose", m_buttonWidth);
    m_buttonWidthAllDesktops = border.readEntry("ButtonWidthAlldesktops", m_buttonWidth);
    m_buttonWidthKeepAbove = border.readEntry("ButtonWidthKeepabove", m_buttonWidth);
    m_buttonWidthKeepBelow = border.readEntry("ButtonWidthKeepbelow", m_buttonWidth);
    m_buttonWidthShade = border.readEntry("ButtonWidthShade", m_buttonWidth);
    m_buttonWidthHelp = border.readEntry("ButtonWidthHelp", m_buttonWidth);
    m_buttonWidthMenu = border.readEntry("ButtonWidthMenu", m_buttonWidth);
    m_buttonHeight = border.readEntry("ButtonHeight", defaultButtonHeight());
    m_buttonSpacing = border.readEntry("ButtonSpacing", defaultButtonSpacing());
    m_buttonMarginTop = border.readEntry("ButtonMarginTop", defaultButtonMarginTop());
    m_explicitButtonSpacer = border.readEntry("ExplicitButtonSpacer", defaultExplicitButtonSpacer());

    m_paddingLeft = border.readEntry("PaddingLeft", defaultPaddingLeft());
    m_paddingRight = border.readEntry("PaddingRight", defaultPaddingRight());
    m_paddingTop = border.readEntry("PaddingTop", defaultPaddingTop());
    m_paddingBottom = border.readEntry("PaddingBottom", defaultPaddingBottom());
}

QColor ThemeConfig::activeTextColor(bool useTabs, bool focused) const
{
    if (!useTabs) {
        return m_activeTextColor;
    }
    if (focused) {
        return m_activeFocusedTextColor;
    } else {
        return m_activeUnfocusedTextColor;
    }
}

QColor ThemeConfig::inactiveTextColor(bool useTabs, bool focused) const
{
    if (!useTabs) {
        return m_inactiveTextColor;
    }
    if (focused) {
        return m_inactiveFocusedTextColor;
    } else {
        return m_inactiveUnfocusedTextColor;
    }
}

} //namespace
