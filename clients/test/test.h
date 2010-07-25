/********************************************************************

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_TEST
#define KWIN_TEST

#include <kdecoration.h>
#include <kdecorationfactory.h>
#include <QPushButton>

namespace KWinTest
{

const int SUPPORTED_WINDOW_TYPES_MASK = NET::NormalMask | NET::DesktopMask | NET::DockMask
    | NET::ToolbarMask | NET::MenuMask | NET::DialogMask /*| NET::OverrideMask*/ | NET::TopMenuMask
    | NET::UtilityMask | NET::SplashMask;

class Decoration
    : public KDecoration
    {
    Q_OBJECT
    public:
	Decoration( KDecorationBridge* bridge, KDecorationFactory* factory );
        virtual void init();
	virtual MousePosition mousePosition( const QPoint& p ) const;
	virtual void borders( int& left, int& right, int& top, int& bottom ) const;
	virtual void resize( const QSize& s );
	virtual QSize minimumSize() const;
        virtual void activeChange() {};
        virtual void captionChange() {};
        virtual void maximizeChange() {};
        virtual void desktopChange() {};
        virtual void shadeChange() {};
        virtual void iconChange() {};
	virtual bool eventFilter( QObject* o, QEvent* e );
        virtual void reset( unsigned long changed );
        virtual bool animateMinimize( bool minimize );
    private:
	QPushButton* button;
    };

class Factory
    : public KDecorationFactory
    {
    public:
        virtual KDecoration* createDecoration( KDecorationBridge* );
        virtual bool reset( unsigned long changed );
    };
    
} // namespace

#endif
