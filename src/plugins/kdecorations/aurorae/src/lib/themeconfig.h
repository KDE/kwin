/*
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef THEMECONFIG_H
#define THEMECONFIG_H
// This class encapsulates all theme config values
// it's a separate class as it's needed by both deco and config dialog

#include <QString>
#include <QColor>

class KConfig;

namespace Aurorae
{
class ThemeConfig
{
public:
    ThemeConfig();
    void load(const KConfig &conf);
    ~ThemeConfig() {};
    // active window
    QColor activeTextColor(bool useTabs = false, bool focused = true) const;
    // inactive window
    QColor inactiveTextColor(bool useTabs = false, bool focused = true) const;
    QColor activeTextShadowColor() const {
        return m_activeTextShadowColor;
    }
    QColor inactiveTextShadowColor() const {
        return m_inactiveTextShadowColor;
    }
    int textShadowOffsetX() const {
        return m_textShadowOffsetX;
    }
    int textShadowOffsetY() const {
        return m_textShadowOffsetY;
    }
    bool useTextShadow() const {
        return m_useTextShadow;
    }
    bool haloActive() const {
        return m_haloActive;
    }
    bool haloInactive() const {
        return m_haloInactive;
    }
    // Alignment
    Qt::Alignment alignment() const {
        return m_alignment;
    };
    Qt::Alignment verticalAlignment() const {
        return m_verticalAlignment;
    }
    int animationTime() const {
        return m_animationTime;
    }
    // Borders
    int borderLeft() const {
        return m_borderLeft;
    }
    int borderRight() const {
        return m_borderRight;
    }
    int borderBottom() const {
        return m_borderBottom;
    }
    int borderTop() const {
        return m_borderTop;
    }

    int titleEdgeTop() const {
        return m_titleEdgeTop;
    }
    int titleEdgeBottom() const {
        return m_titleEdgeBottom;
    }
    int titleEdgeLeft() const {
        return m_titleEdgeLeft;
    }
    int titleEdgeRight() const {
        return m_titleEdgeRight;
    }
    int titleEdgeTopMaximized() const {
        return m_titleEdgeTopMaximized;
    }
    int titleEdgeBottomMaximized() const {
        return m_titleEdgeBottomMaximized;
    }
    int titleEdgeLeftMaximized() const {
        return m_titleEdgeLeftMaximized;
    }
    int titleEdgeRightMaximized() const {
        return m_titleEdgeRightMaximized;
    }
    int titleBorderLeft() const {
        return m_titleBorderLeft;
    }
    int titleBorderRight() const {
        return m_titleBorderRight;
    }
    int titleHeight() const {
        return m_titleHeight;
    }

    int buttonWidth() const {
        return m_buttonWidth;
    }
    int buttonWidthMinimize() const {
        return m_buttonWidthMinimize;
    }
    int buttonWidthMaximizeRestore() const {
        return m_buttonWidthMaximizeRestore;
    }
    int buttonWidthClose() const {
        return m_buttonWidthClose;
    }
    int buttonWidthAllDesktops() const {
        return m_buttonWidthAllDesktops;
    }
    int buttonWidthKeepAbove() const {
        return m_buttonWidthKeepAbove;
    }
    int buttonWidthKeepBelow() const {
        return m_buttonWidthKeepBelow;
    }
    int buttonWidthShade() const {
        return m_buttonWidthShade;
    }
    int buttonWidthHelp() const {
        return m_buttonWidthHelp;
    }
    int buttonWidthMenu() const {
        return m_buttonWidthMenu;
    }
    int buttonWidthAppMenu() const {
        return m_buttonWidthAppMenu;
    }
    int buttonHeight() const {
        return m_buttonHeight;
    }
    int buttonSpacing() const {
        return m_buttonSpacing;
    }
    int buttonMarginTop() const {
        return m_buttonMarginTop;
    }
    int explicitButtonSpacer() const {
        return m_explicitButtonSpacer;
    }

    int paddingLeft() const {
        return m_paddingLeft;
    }
    int paddingRight() const {
        return m_paddingRight;
    }
    int paddingTop() const {
        return m_paddingTop;
    }
    int paddingBottom() const {
        return m_paddingBottom;
    }

    bool shadow() const {
        return m_shadow;
    }

    int decorationPosition() const {
        return m_decorationPosition;
    }

    static QColor defaultActiveTextColor() {
        return QColor(Qt::black);
    }
    static QColor defaultActiveFocusedTextColor() {
        return QColor(Qt::black);
    }
    static QColor defaultActiveUnfocusedTextColor() {
        return QColor(Qt::black);
    }
    static QColor defaultInactiveTextColor() {
        return QColor(Qt::black);
    }
    static QColor defaultInactiveFocusedTextColor() {
        return QColor(Qt::black);
    }
    static QColor defaultInactiveUnfocusedTextColor() {
        return QColor(Qt::black);
    }
    static QColor defaultActiveTextShadowColor() {
        return QColor(Qt::white);
    }
    static QColor defaultInactiveTextShadowColor() {
        return QColor(Qt::white);
    }
    static int defaultTextShadowOffsetX() {
        return 0;
    }
    static int defaultTextShadowOffsetY() {
        return 0;
    }
    static bool defaultUseTextShadow() {
        return false;
    }
    static bool defaultHaloActive() {
        return false;
    }
    static bool defaultHaloInactive() {
        return false;
    }
    static Qt::Alignment defaultAlignment() {
        return Qt::AlignLeft;
    }
    static Qt::Alignment defaultVerticalAlignment() {
        return Qt::AlignVCenter;
    }
    // borders
    static int defaultBorderLeft() {
        return 5;
    }
    static int defaultBorderRight() {
        return 5;
    }
    static int defaultBorderBottom() {
        return 5;
    }
    static int defaultBorderTop() {
        return 0;
    }
    // title
    static int defaultTitleEdgeTop() {
        return 5;
    }
    static int defaultTitleEdgeBottom() {
        return 5;
    }
    static int defaultTitleEdgeLeft() {
        return 5;
    }
    static int defaultTitleEdgeRight() {
        return 5;
    }
    static int defaultTitleEdgeTopMaximized() {
        return 0;
    }
    static int defaultTitleEdgeBottomMaximized() {
        return 0;
    }
    static int defaultTitleEdgeLeftMaximized() {
        return 0;
    }
    static int defaultTitleEdgeRightMaximized() {
        return 0;
    }
    static int defaultTitleBorderLeft() {
        return 5;
    }
    static int defaultTitleBorderRight() {
        return 5;
    }
    static int defaultTitleHeight() {
        return 20;
    }
    // buttons
    static int defaultButtonWidth() {
        return 20;
    }
    static int defaultButtonWidthMinimize() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthMaximizeRestore() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthClose() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthAllDesktops() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthKeepAbove() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthKeepBelow() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthShade() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthHelp() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthMenu() {
        return defaultButtonWidth();
    }
    static int defaultButtonWidthAppMenu() {
        return defaultButtonWidthMenu();
    }
    static int defaultButtonHeight() {
        return 20;
    }
    static int defaultButtonSpacing() {
        return 5;
    }
    static int defaultButtonMarginTop() {
        return 0;
    }
    static int defaultExplicitButtonSpacer() {
        return 10;
    }
    // padding
    static int defaultPaddingLeft() {
        return 0;
    }
    static int defaultPaddingRight() {
        return 0;
    }
    static int defaultPaddingTop() {
        return 0;
    }
    static int defaultPaddingBottom() {
        return 0;
    }
    static int defaultAnimationTime() {
        return 0;
    }
    static bool defaultShadow() {
        return true;
    }
    static int defaultDecorationPosition() {
        return 0;
    }

private:
    QColor m_activeTextColor;
    QColor m_activeFocusedTextColor;
    QColor m_activeUnfocusedTextColor;
    QColor m_inactiveTextColor;
    QColor m_inactiveFocusedTextColor;
    QColor m_inactiveUnfocusedTextColor;
    QColor m_activeTextShadowColor;
    QColor m_inactiveTextShadowColor;
    int m_textShadowOffsetX;
    int m_textShadowOffsetY;
    bool m_useTextShadow;
    bool m_haloActive;
    bool m_haloInactive;
    Qt::Alignment m_alignment;
    Qt::Alignment m_verticalAlignment;
    // borders
    int m_borderLeft;
    int m_borderRight;
    int m_borderBottom;
    int m_borderTop;

    // title
    int m_titleEdgeTop;
    int m_titleEdgeBottom;
    int m_titleEdgeLeft;
    int m_titleEdgeRight;
    int m_titleEdgeTopMaximized;
    int m_titleEdgeBottomMaximized;
    int m_titleEdgeLeftMaximized;
    int m_titleEdgeRightMaximized;
    int m_titleBorderLeft;
    int m_titleBorderRight;
    int m_titleHeight;

    // buttons
    int m_buttonWidth;
    int m_buttonWidthMinimize;
    int m_buttonWidthMaximizeRestore;
    int m_buttonWidthClose;
    int m_buttonWidthAllDesktops;
    int m_buttonWidthKeepAbove;
    int m_buttonWidthKeepBelow;
    int m_buttonWidthShade;
    int m_buttonWidthHelp;
    int m_buttonWidthMenu;
    int m_buttonWidthAppMenu;
    int m_buttonHeight;
    int m_buttonSpacing;
    int m_buttonMarginTop;
    int m_explicitButtonSpacer;

    // padding
    int m_paddingLeft;
    int m_paddingRight;
    int m_paddingTop;
    int m_paddingBottom;

    int m_animationTime;

    bool m_shadow;

    int m_decorationPosition;
};

}

#endif
