/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#include "kdecoration_p.h"

#include <kconfig.h>
#include <kconfiggroup.h>
#include <QFontDatabase>
#include <QPalette>
#include <QApplication>
#include <assert.h>

KDecorationOptionsPrivate::KDecorationOptionsPrivate(KDecorationOptions *parent)
    : q(parent)
    , title_buttons_left(KDecorationOptions::defaultTitleButtonsLeft())
    , title_buttons_right(KDecorationOptions::defaultTitleButtonsRight())
    , custom_button_positions(false)
    , show_tooltips(true)
    , border_size(BorderNormal)
    , cached_border_size(BordersCount)   // invalid
    , opMaxButtonRightClick(MaximizeOp)
    , opMaxButtonMiddleClick(VMaximizeOp)
    , opMaxButtonLeftClick(HMaximizeOp)
{
    for (int i = 0; i < NUM_COLORS * 2; ++i)
        pal[i] = nullptr;
}

KDecorationOptionsPrivate::~KDecorationOptionsPrivate()
{
    int i;
    for (i = 0; i < NUM_COLORS * 2; ++i) {
        if (pal[i]) {
            delete pal[i];
            pal[i] = nullptr;
        }
    }
}

void KDecorationOptionsPrivate::updateSettings(KConfig* config)
{
    KConfigGroup wmConfig(config, "WM");

// SettingColors
    QColor old_colors[NUM_COLORS*2];
    for (int i = 0;
            i < NUM_COLORS * 2;
            ++i)
        old_colors[ i ] = colors[ i ];

    QPalette appPal = QApplication::palette();
    // normal colors
    colors[ColorFrame] = appPal.color(QPalette::Active, QPalette::Background);
    colors[ColorFrame] = wmConfig.readEntry("frame", colors[ColorFrame]);
    colors[ColorHandle] = colors[ColorFrame];
    colors[ColorHandle] = wmConfig.readEntry("handle", colors[ColorHandle]);

    // full button configuration (background, blend, and foreground
    if (QPixmap::defaultDepth() > 8)
        colors[ColorButtonBg] = colors[ColorFrame].light(130);
    else
        colors[ColorButtonBg] = colors[ColorFrame];
    colors[ColorButtonBg] = wmConfig.readEntry("activeTitleBtnBg",
                            colors[ColorFrame]);
    colors[ColorTitleBar] = appPal.color(QPalette::Active, QPalette::Highlight);
    colors[ColorTitleBar] = wmConfig.readEntry("activeBackground",
                            colors[ColorTitleBar]);
    if (QPixmap::defaultDepth() > 8)
        colors[ColorTitleBlend] = colors[ ColorTitleBar ].dark(110);
    else
        colors[ColorTitleBlend] = colors[ ColorTitleBar ];
    colors[ColorTitleBlend] = wmConfig.readEntry("activeBlend",
                              colors[ColorTitleBlend]);

    colors[ColorFont] = appPal.color(QPalette::Active, QPalette::HighlightedText);
    colors[ColorFont] = wmConfig.readEntry("activeForeground", colors[ColorFont]);

    // inactive
    colors[ColorFrame+NUM_COLORS] = wmConfig.readEntry("inactiveFrame",
                                    colors[ColorFrame]);
    colors[ColorTitleBar+NUM_COLORS] = colors[ColorFrame];
    colors[ColorTitleBar+NUM_COLORS] = wmConfig.
                                       readEntry("inactiveBackground", colors[ColorTitleBar+NUM_COLORS]);

    if (QPixmap::defaultDepth() > 8)
        colors[ColorTitleBlend+NUM_COLORS] = colors[ ColorTitleBar+NUM_COLORS ].dark(110);
    else
        colors[ColorTitleBlend+NUM_COLORS] = colors[ ColorTitleBar+NUM_COLORS ];
    colors[ColorTitleBlend+NUM_COLORS] =
        wmConfig.readEntry("inactiveBlend", colors[ColorTitleBlend+NUM_COLORS]);

    // full button configuration
    if (QPixmap::defaultDepth() > 8)
        colors[ColorButtonBg+NUM_COLORS] = colors[ColorFrame+NUM_COLORS].light(130);
    else
        colors[ColorButtonBg+NUM_COLORS] = colors[ColorFrame+NUM_COLORS];
    colors[ColorButtonBg+NUM_COLORS] =
        wmConfig.readEntry("inactiveTitleBtnBg",
                           colors[ColorButtonBg]);

    colors[ColorHandle+NUM_COLORS] =
        wmConfig.readEntry("inactiveHandle", colors[ColorHandle]);

    colors[ColorFont+NUM_COLORS] = colors[ColorFrame].dark();
    colors[ColorFont+NUM_COLORS] = wmConfig.readEntry("inactiveForeground",
                                   colors[ColorFont+NUM_COLORS]);

    bool colorsChanged = false;
    for (int i = 0;
            i < NUM_COLORS * 2;
            ++i)
        if (old_colors[ i ] != colors[ i ]) {
            colorsChanged = true;
            break;
        }
    if (colorsChanged) {
        emit q->colorsChanged();
    }

// SettingFont
    QFont old_activeFont = activeFont;
    QFont old_inactiveFont = inactiveFont;
    QFont old_activeFontSmall = activeFontSmall;
    QFont old_inactiveFontSmall = inactiveFontSmall;

    QFont activeFontGuess = QFontDatabase::systemFont(QFontDatabase::TitleFont);

    activeFont = wmConfig.readEntry("activeFont", activeFontGuess);
    if (activeFont != old_activeFont) {
        emit q->activeFontChanged();
    }
    inactiveFont = wmConfig.readEntry("inactiveFont", activeFont);
    if (inactiveFont != old_inactiveFont) {
        emit q->inactiveFontChanged();
    }

    activeFontSmall = activeFont;
    // TODO: Is it useful ? (Temporary hack)
    //activeFontSmall.setPointSize(activeFont.pointSize() - 2 > 0 ? activeFont.pointSize() - 2 : activeFont.pointSize()+1 );
    activeFontSmall = wmConfig.readEntry("activeFontSmall", activeFontSmall);
    if (activeFontSmall != old_activeFontSmall) {
        emit q->smallActiveFontChanged();
    }
    inactiveFontSmall = wmConfig.readEntry("inactiveFontSmall", activeFontSmall);
    if (inactiveFontSmall != old_inactiveFontSmall) {
        emit q->smallInactiveFontChanged();
    }

    KConfigGroup styleConfig(config, "Style");
// SettingsButtons
    const auto old_title_buttons_left = title_buttons_left;
    const auto old_title_buttons_right = title_buttons_right;
    bool old_custom_button_positions = custom_button_positions;
    custom_button_positions = styleConfig.readEntry("CustomButtonPositions", false);
    if (custom_button_positions) {
        title_buttons_left  = q->readDecorationButtons(styleConfig, "ButtonsOnLeft",
                                                       KDecorationOptions::defaultTitleButtonsLeft());
        title_buttons_right = q->readDecorationButtons(styleConfig, "ButtonsOnRight",
                                                       KDecorationOptions::defaultTitleButtonsRight());
    } else {
        title_buttons_left  = KDecorationOptions::defaultTitleButtonsLeft();
        title_buttons_right = KDecorationOptions::defaultTitleButtonsRight();
    }
    if (old_custom_button_positions != custom_button_positions) {
        emit q->customButtonPositionsChanged();
    }
    if (old_title_buttons_left != title_buttons_left) {
        emit q->leftButtonsChanged();
    }
    if (old_title_buttons_right != title_buttons_right) {
        emit q->rightButtonsChanged();
    }

// SettingTooltips
    bool old_show_tooltips = show_tooltips;
    show_tooltips = styleConfig.readEntry("ShowToolTips", true);
    if (old_show_tooltips != show_tooltips) {
        emit q->showTooltipsChanged();
    }

// SettingBorder

    BorderSize old_border_size = border_size;
    int border_size_num = styleConfig.readEntry("BorderSize", (int)BorderNormal);
    if (border_size_num >= 0 && border_size_num < BordersCount)
        border_size = static_cast< BorderSize >(border_size_num);
    else
        border_size = BorderNormal;
    if (old_border_size != border_size) {
        emit q->borderSizeChanged();
    }
    cached_border_size = BordersCount; // invalid

// destroy cached values
    int i;
    for (i = 0; i < NUM_COLORS * 2; ++i) {
        if (pal[i]) {
            delete pal[i];
            pal[i] = nullptr;
        }
    }

    emit q->configChanged();
}

KDecorationDefines::BorderSize KDecorationOptionsPrivate::findPreferredBorderSize(BorderSize size,
        QList< BorderSize > sizes) const
{
    for (QList< BorderSize >::ConstIterator it = sizes.constBegin();
            it != sizes.constEnd();
            ++it)
        if (size <= *it)   // size is either a supported size, or *it is the closest larger supported
            return *it;
    return sizes.last(); // size is larger than all supported ones, return largest
}
