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

#ifndef KNIFTY_H
#define KNIFTY_H

#include <kdecoration.h>
#include <kdecorationfactory.h>

namespace KWinPlastik {

#include <qfont.h>

enum ColorType {
    WindowContour=0,
    TitleGradientFrom,
    TitleGradientTo,
    TitleGradientToTop,
    TitleHighlightTop,
    SideHighlightLeft,
    SideHighlightRight,
    SideHighlightBottom,
    Border,
    TitleFont
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

    static bool initialized() { return m_initialized; }

    static int  titleHeight() { return m_titleHeight; }
    static int  titleHeightTool() { return m_titleHeightTool; }
    static QFont titleFont() { return m_titleFont; }
    static QFont titleFontTool() { return m_titleFontTool; }
    static bool titleShadow() { return m_titleShadow; }
    static int  borderSize() { return m_borderSize; }
    static bool animateButtons() { return m_animateButtons; }
    static bool menuClose() { return m_menuClose; }
    static Qt::AlignmentFlags titleAlign() { return m_titleAlign; }
    static bool reverseLayout() { return m_reverse; }
    static QColor getColor(KWinPlastik::ColorType type, const bool active = true);
    QValueList< PlastikHandler::BorderSize >  borderSizes() const;
private:
    void readConfig();

    static bool m_titleShadow;
    static bool m_shrinkBorders;
    static bool m_animateButtons;
    static bool m_menuClose;
    static bool m_reverse;
    static int  m_borderSize;
    static int  m_titleHeight;
    static int  m_titleHeightTool;
    static QFont m_titleFont;
    static QFont m_titleFontTool;
    static Qt::AlignmentFlags m_titleAlign;

    static bool m_initialized;
};

} // KWinPlastik

#endif // KNIFT_H
