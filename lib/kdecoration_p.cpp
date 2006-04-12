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
#include <kglobalsettings.h>
#include <qpalette.h>
#include <qapplication.h>
#include <assert.h>

KDecorationOptionsPrivate::KDecorationOptionsPrivate()
    {
    for(int i=0; i < NUM_COLORS*2; ++i)
        pal[i] = NULL;
    }

KDecorationOptionsPrivate::~KDecorationOptionsPrivate()
    {
    int i;
    for(i=0; i < NUM_COLORS*2; ++i)
        {
        if(pal[i])
            {
            delete pal[i];
            pal[i] = NULL;
            }
        }
    }

void KDecorationOptionsPrivate::defaultKWinSettings()
    {
    title_buttons_left = "MS";
    title_buttons_right = "HIAX";
    custom_button_positions = false;
    show_tooltips = true;
    border_size = BorderNormal;
    cached_border_size = BordersCount; // invalid
    move_resize_maximized_windows = true;
    OpMaxButtonRightClick = MaximizeOp;
    OpMaxButtonMiddleClick = VMaximizeOp;
    OpMaxButtonLeftClick = HMaximizeOp;
    }

unsigned long KDecorationOptionsPrivate::updateKWinSettings( KConfig* config )
    {
    unsigned long changed = 0;
    QString old_group = config->group();
    config->setGroup( "WM" );

// SettingColors
    QColor old_colors[NUM_COLORS*2];
    for( int i = 0;
         i < NUM_COLORS*2;
         ++i )
        old_colors[ i ] = colors[ i ];
        
    QPalette appPal = QApplication::palette();
    // normal colors
    colors[ColorFrame] = appPal.color( QPalette::Active, QPalette::Background );
    colors[ColorFrame] = config->readEntry("frame", colors[ColorFrame]);
    colors[ColorHandle] = colors[ColorFrame];
    colors[ColorHandle] = config->readEntry("handle", colors[ColorHandle]);

    // full button configuration (background, blend, and foreground
    if(QPixmap::defaultDepth() > 8)
        colors[ColorButtonBg] = colors[ColorFrame].light(130);
    else
        colors[ColorButtonBg] = colors[ColorFrame];
    colors[ColorButtonBg] = config->readEntry("activeTitleBtnBg",
                                              colors[ColorFrame]);
    colors[ColorTitleBar] = appPal.color( QPalette::Active, QPalette::Highlight );
    colors[ColorTitleBar] = config->readEntry("activeBackground",
                                              colors[ColorTitleBar]);
    if(QPixmap::defaultDepth() > 8)
        colors[ColorTitleBlend] = colors[ ColorTitleBar ].dark(110);
    else
        colors[ColorTitleBlend] = colors[ ColorTitleBar ];
    colors[ColorTitleBlend] = config->readEntry("activeBlend",
                                                colors[ColorTitleBlend]);

    colors[ColorFont] = appPal.color( QPalette::Active, QPalette::HighlightedText );
    colors[ColorFont] = config->readEntry("activeForeground", colors[ColorFont]);

    // inactive
    colors[ColorFrame+NUM_COLORS] = config->readEntry("inactiveFrame",
                                                      colors[ColorFrame]);
    colors[ColorTitleBar+NUM_COLORS] = colors[ColorFrame];
    colors[ColorTitleBar+NUM_COLORS] = config->
        readEntry("inactiveBackground", colors[ColorTitleBar+NUM_COLORS]);

    if(QPixmap::defaultDepth() > 8)
        colors[ColorTitleBlend+NUM_COLORS] = colors[ ColorTitleBar+NUM_COLORS ].dark(110);
    else
        colors[ColorTitleBlend+NUM_COLORS] = colors[ ColorTitleBar+NUM_COLORS ];
    colors[ColorTitleBlend+NUM_COLORS] =
        config->readEntry("inactiveBlend", colors[ColorTitleBlend+NUM_COLORS]);

    // full button configuration
    if(QPixmap::defaultDepth() > 8)
        colors[ColorButtonBg+NUM_COLORS] = colors[ColorFrame+NUM_COLORS].light(130);
    else
        colors[ColorButtonBg+NUM_COLORS] = colors[ColorFrame+NUM_COLORS];
    colors[ColorButtonBg+NUM_COLORS] =
        config->readEntry("inactiveTitleBtnBg",
                               colors[ColorButtonBg]);

    colors[ColorHandle+NUM_COLORS] =
        config->readEntry("inactiveHandle", colors[ColorHandle]);

    colors[ColorFont+NUM_COLORS] = colors[ColorFrame].dark();
    colors[ColorFont+NUM_COLORS] = config->readEntry("inactiveForeground",
                                                     colors[ColorFont+NUM_COLORS]);

    for( int i = 0;
         i < NUM_COLORS*2;
         ++i )
        if( old_colors[ i ] != colors[ i ] )
            changed |= SettingColors;

// SettingFont
    QFont old_activeFont = activeFont;
    QFont old_inactiveFont = inactiveFont;
    QFont old_activeFontSmall = activeFontSmall;
    QFont old_inactiveFontSmall = inactiveFontSmall;

    QFont activeFontGuess = KGlobalSettings::generalFont();
    activeFontGuess.setBold(true);
    activeFontGuess.setPixelSize(12);

    activeFont = config->readEntry("activeFont", activeFontGuess);
    inactiveFont = config->readEntry("inactiveFont", activeFont);

    activeFontSmall = activeFont;
#warning KDE4 : is it usefull ? ( temporary hack )
//    activeFontSmall.setPointSize(activeFont.pointSize() - 2 > 0 ? activeFont.pointSize() - 2 : activeFont.pointSize()+1 );
    activeFontSmall = config->readEntry("activeFontSmall", activeFontSmall);
    inactiveFontSmall = config->readEntry("inactiveFontSmall", activeFontSmall);

    if( old_activeFont != activeFont
        || old_inactiveFont != inactiveFont
        || old_activeFontSmall != activeFontSmall
        || old_inactiveFontSmall != inactiveFontSmall )
        changed |= SettingFont;

    config->setGroup( "Style" );
// SettingsButtons        
    QString old_title_buttons_left = title_buttons_left;
    QString old_title_buttons_right = title_buttons_right;
    bool old_custom_button_positions = custom_button_positions;
    custom_button_positions = config->readEntry("CustomButtonPositions", QVariant(false)).toBool();
    if (custom_button_positions)
        {
        title_buttons_left  = config->readEntry("ButtonsOnLeft", "MS");
        title_buttons_right = config->readEntry("ButtonsOnRight", "HIAX");
        }
    else
        {
        title_buttons_left  = "MS";
        title_buttons_right = "HIAX";
        }
    if( old_custom_button_positions != custom_button_positions
        || ( custom_button_positions &&
                ( old_title_buttons_left != title_buttons_left
                || old_title_buttons_right != title_buttons_right )))
        changed |= SettingButtons;

// SettingTooltips
    bool old_show_tooltips = show_tooltips;
    show_tooltips = config->readEntry("ShowToolTips", QVariant(true)).toBool();
    if( old_show_tooltips != show_tooltips )
        changed |= SettingTooltips;

// SettingBorder

    BorderSize old_border_size = border_size;
    int border_size_num = config->readEntry( "BorderSize", (int)BorderNormal );
    if( border_size_num >= 0 && border_size_num < BordersCount )
        border_size = static_cast< BorderSize >( border_size_num );
    else
        border_size = BorderNormal;
    if( old_border_size != border_size )
        changed |= SettingBorder;
    cached_border_size = BordersCount; // invalid

    config->setGroup( "Windows" );
    bool old_move_resize_maximized_windows = move_resize_maximized_windows;
    move_resize_maximized_windows = config->readEntry( "MoveResizeMaximizedWindows", QVariant(false )).toBool();
    if( old_move_resize_maximized_windows != move_resize_maximized_windows )
        changed |= SettingBorder;

// destroy cached values
    int i;
    for(i=0; i < NUM_COLORS*2; ++i)
        {
        if(pal[i])
            {
            delete pal[i];
            pal[i] = NULL;
            }
        }

    config->setGroup( old_group );

    return changed;
    }

KDecorationDefines::BorderSize KDecorationOptionsPrivate::findPreferredBorderSize( BorderSize size,
    QList< BorderSize > sizes ) const
    {
    for( QList< BorderSize >::ConstIterator it = sizes.begin();
         it != sizes.end();
         ++it )
        if( size <= *it ) // size is either a supported size, or *it is the closest larger supported
            return *it;
    return sizes.last(); // size is larger than all supported ones, return largest
    }
