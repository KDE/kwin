/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

#ifndef PLASTIK_H
#define PLASTIK_H

#include <qfont.h>

#include <kdecoration.h>
#include <kdecorationfactory.h>

namespace KWinPlastik {

enum ColorType {
    WindowContour=0,
    TitleGradient1, // top
    TitleGradient2,
    TitleGradient3, // bottom
    TitleHighlightTop,
    SideHighlightLeft,
    SideHighlightRight,
    SideHighlightBottom,
    Border,
    TitleFont
};

enum Pixmaps {
    TitleBarTileTop=0,
    TitleBarTile,
    NumPixmaps
};

enum ButtonIcon {
    CloseIcon = 0,
    MaxIcon,
    MaxRestoreIcon,
    MinIcon,
    HelpIcon,
    OnAllDesktopsIcon,
    NotOnAllDesktopsIcon,
    KeepAboveIcon,
    NoKeepAboveIcon,
    KeepBelowIcon,
    NoKeepBelowIcon,
    ShadeIcon,
    UnShadeIcon,
    NumButtonIcons
};

class PlastikHandler: public QObject, public KDecorationFactory
{
    Q_OBJECT
public:
    PlastikHandler();
    ~PlastikHandler();
    virtual bool reset( unsigned long changed );

    virtual KDecoration* createDecoration( KDecorationBridge* );
    virtual bool supports( Ability ability );

    const QPixmap &pixmap(Pixmaps type, bool active, bool toolWindow);
    const QBitmap &buttonBitmap(ButtonIcon type, const QSize &size, bool toolWindow);

    int  titleHeight() { return m_titleHeight; }
    int  titleHeightTool() { return m_titleHeightTool; }
    const QFont &titleFont() { return m_titleFont; }
    const QFont &titleFontTool() { return m_titleFontTool; }
    bool titleShadow() { return m_titleShadow; }
    int  borderSize() { return m_borderSize; }
    bool animateButtons() { return m_animateButtons; }
    bool menuClose() { return m_menuClose; }
    Qt::AlignmentFlags titleAlign() { return m_titleAlign; }
    bool reverseLayout() { return m_reverse; }
    QColor getColor(KWinPlastik::ColorType type, const bool active = true);

    QValueList< PlastikHandler::BorderSize >  borderSizes() const;
private:
    void readConfig();

    bool m_titleShadow;
    bool m_shrinkBorders;
    bool m_animateButtons;
    bool m_menuClose;
    bool m_reverse;
    int  m_borderSize;
    int  m_titleHeight;
    int  m_titleHeightTool;
    QFont m_titleFont;
    QFont m_titleFontTool;
    Qt::AlignmentFlags m_titleAlign;

    // pixmap cache
    QPixmap *m_pixmaps[2][2][NumPixmaps]; // button pixmaps have normal+pressed state...
    QBitmap *m_bitmaps[2][NumButtonIcons];
};

PlastikHandler* Handler();

} // KWinPlastik

#endif // PLASTIK_H
