#ifndef oxygenbutton_h
#define oxygenbutton_h

//////////////////////////////////////////////////////////////////////////////
// Button.h
// -------------------
//
// Copyright (c) 2006, 2007 Riccardo Iaconelli <riccardo@kde.org>
// Copyright (c) 2006, 2007 Casper Boemann <cbr@boemann.dk>
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include <kcommondecoration.h>

#include "oxygenfactory.h"
#include "oxygenanimation.h"

namespace Oxygen
{
    class Client;

    enum ButtonStatus {
        Normal,
        Hovered,
        Pressed
    };

    Q_DECLARE_FLAGS(ButtonState, ButtonStatus)

        class Button : public KCommonDecorationButton
    {

        Q_OBJECT

            //! declare animation progress property
            Q_PROPERTY( qreal glowIntensity READ glowIntensity WRITE setGlowIntensity )

            public:

            //! constructor
            explicit Button(Client &parent,
            const QString &tip=QString(),
            ButtonType type=ButtonHelp );

        //! destructor
        ~Button();

        //! destructor
        QSize sizeHint() const;

        //! button type
        ButtonType type( void ) const
        { return _type; }

        //! set force inactive
        /*! returns true if value was actually changed */
        void setForceInactive( const bool& value )
        { _forceInactive = value; }

        //! configuration reset
        virtual void reset( unsigned long );

        //!@name glow animation
        //@{
        void setGlowIntensity( qreal value )
        {
            if( _glowIntensity == value ) return;
            _glowIntensity = value;
            update();
        }

        qreal glowIntensity( void ) const
        { return _glowIntensity; }

        //@}

        protected:

        //! press event
        void mousePressEvent( QMouseEvent* );

        //! release event
        void mouseReleaseEvent( QMouseEvent* );

        //! enter event
        void enterEvent( QEvent* );

        //! leave event
        void leaveEvent( QEvent* );

        //! resize event
        void resizeEvent( QResizeEvent* );

        //! paint
        void paintEvent( QPaintEvent* );

        //! render buttn to provided painter
        void paint(QPainter& );

        //! draw icon
        void drawIcon( QPainter* );

        //! color
        QColor buttonDetailColor( const QPalette& ) const;

        //! color
        QColor buttonDetailColor( const QPalette& palette, bool active ) const
        {
            if( _type == ButtonItemClose )
            {

                return active ?
                    palette.color(QPalette::Active, QPalette::WindowText ):
                    _helper.inactiveTitleBarTextColor( palette );

            } else {

                return active ?
                    palette.color(QPalette::Active, QPalette::ButtonText ):
                    _helper.inactiveButtonTextColor( palette );

            }

        }

        //! true if animation is in progress
        bool isAnimated( void ) const
        { return _glowAnimation->isRunning(); }

        //! true if button is active
        bool isActive( void ) const;

        //! true if buttons hover are animated
        bool animateButtonHover( void ) const;

        //!@name button properties
        //@{

        //! true if button if of menu type
        bool isMenuButton( void ) const
        { return _type == ButtonMenu || _type == ButtonItemMenu; }

        //! true if button is of toggle type
        bool isToggleButton( void ) const
        { return _type == ButtonSticky || _type == ButtonAbove || _type == ButtonBelow; }

        //! true if button if of close type
        bool isCloseButton( void ) const
        { return _type == ButtonClose || _type == ButtonItemClose; }

        //! true if button has decoration
        bool hasDecoration( void ) const
        { return !isMenuButton() && _type != ButtonItemClose; }

        //@}

        private:

        //! parent client
        const Client &_client;

        //! helper
        DecoHelper &_helper;

        //! backing store pixmap (when compositing is not active)
        QPixmap _pixmap;

        //! button type
        ButtonType _type;

        //! button status
        ButtonState _status;

        //! true if button should be forced inactive
        bool _forceInactive;

        //! glow animation
        //Animation::Pointer _glowAnimation;
        Animation* _glowAnimation;

        //! glow intensity
        qreal _glowIntensity;


    };

} //namespace Oxygen

#endif
