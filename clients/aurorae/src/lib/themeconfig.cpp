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
{
}

void ThemeConfig::load(const KConfig &conf)
{
    KConfigGroup general(&conf, "General");
    m_activeTextColor = general.readEntry("ActiveTextColor", QColor(Qt::black));
    m_inactiveTextColor = general.readEntry("InactiveTextColor", QColor(Qt::black));
    m_activeFocusedTextColor = general.readEntry("ActiveFocusedTabColor", m_activeTextColor);
    m_activeUnfocusedTextColor = general.readEntry("ActiveUnfocusedTabColor", m_inactiveTextColor);
    m_inactiveFocusedTextColor = general.readEntry("InactiveFocusedTabColor", m_inactiveTextColor);
    m_inactiveUnfocusedTextColor = general.readEntry("InactiveUnfocusedTabColor", m_inactiveTextColor);
    m_useTextShadow = general.readEntry("UseTextShadow", false);
    m_activeTextShadowColor = general.readEntry("ActiveTextShadowColor", QColor(Qt::white));
    m_inactiveTextShadowColor = general.readEntry("InactiveTextShadowColor", QColor(Qt::white));
    m_textShadowOffsetX = general.readEntry("TextShadowOffsetX", 0);
    m_textShadowOffsetY = general.readEntry("TextShadowOffsetY", 0);
    m_haloActive = general.readEntry("HaloActive", false);
    m_haloInactive = general.readEntry("HaloInactive", false);
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
    m_animationTime = general.readEntry("Animation", 0);
    m_defaultButtonsLeft = general.readEntry("LeftButtons", KDecorationOptions::defaultTitleButtonsLeft());
    m_defaultButtonsRight = general.readEntry("RightButtons", KDecorationOptions::defaultTitleButtonsRight());
    m_shadow = general.readEntry("Shadow", true);
    m_decorationPosition = general.readEntry("DecorationPosition", 0);

    KConfigGroup border(&conf, "Layout");
    // default values taken from KCommonDecoration::layoutMetric() in kcommondecoration.cpp
    m_borderLeft = border.readEntry("BorderLeft", 5);
    m_borderRight = border.readEntry("BorderRight", 5);
    m_borderBottom = border.readEntry("BorderBottom", 5);
    m_borderTop = border.readEntry("BorderTop", 0);

    m_titleEdgeTop = border.readEntry("TitleEdgeTop", 5);
    m_titleEdgeBottom = border.readEntry("TitleEdgeBottom", 5);
    m_titleEdgeLeft = border.readEntry("TitleEdgeLeft", 5);
    m_titleEdgeRight = border.readEntry("TitleEdgeRight", 5);
    m_titleEdgeTopMaximized = border.readEntry("TitleEdgeTopMaximized", 0);
    m_titleEdgeBottomMaximized = border.readEntry("TitleEdgeBottomMaximized", 0);
    m_titleEdgeLeftMaximized = border.readEntry("TitleEdgeLeftMaximized", 0);
    m_titleEdgeRightMaximized = border.readEntry("TitleEdgeRightMaximized", 0);
    m_titleBorderLeft = border.readEntry("TitleBorderLeft", 5);
    m_titleBorderRight = border.readEntry("TitleBorderRight", 5);
    m_titleHeight = border.readEntry("TitleHeight", 20);

    m_buttonWidth = border.readEntry("ButtonWidth", 20);
    m_buttonWidthMinimize = border.readEntry("ButtonWidthMinimize", m_buttonWidth);
    m_buttonWidthMaximizeRestore = border.readEntry("ButtonWidthMaximizeRestore", m_buttonWidth);
    m_buttonWidthClose = border.readEntry("ButtonWidthClose", m_buttonWidth);
    m_buttonWidthAllDesktops = border.readEntry("ButtonWidthAlldesktops", m_buttonWidth);
    m_buttonWidthKeepAbove = border.readEntry("ButtonWidthKeepabove", m_buttonWidth);
    m_buttonWidthKeepBelow = border.readEntry("ButtonWidthKeepbelow", m_buttonWidth);
    m_buttonWidthShade = border.readEntry("ButtonWidthShade", m_buttonWidth);
    m_buttonWidthHelp = border.readEntry("ButtonWidthHelp", m_buttonWidth);
    m_buttonWidthMenu = border.readEntry("ButtonWidthMenu", m_buttonWidth);
    m_buttonHeight = border.readEntry("ButtonHeight", 20);
    m_buttonSpacing = border.readEntry("ButtonSpacing", 5);
    m_buttonMarginTop = border.readEntry("ButtonMarginTop", 0);
    m_explicitButtonSpacer = border.readEntry("ExplicitButtonSpacer", 10);

    m_paddingLeft = border.readEntry("PaddingLeft", 0);
    m_paddingRight = border.readEntry("PaddingRight", 0);
    m_paddingTop = border.readEntry("PaddingTop", 0);
    m_paddingBottom = border.readEntry("PaddingBottom", 0);
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
