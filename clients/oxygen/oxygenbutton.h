#ifndef Button_h
#define Button_h

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
            const QString &tip=NULL,
            ButtonType type=ButtonHelp );

        //! destructor
        ~Button();

        //! destructor
        QSize sizeHint() const;

        //! button type
        ButtonType type( void ) const
        { return type_; }

        //! set force inactive
        /*! returns true if value was actually changed */
        void setForceInactive( const bool& value )
        { forceInactive_ = value; }

        //! configuration reset
        virtual void reset( unsigned long );

        //!@name glow animation
        //@{
        void setGlowIntensity( qreal value )
        {
            if( glowIntensity_ == value ) return;
            glowIntensity_ = value;
            update();
        }

        qreal glowIntensity( void ) const
        { return glowIntensity_; }

        //@}

        protected:

        //! press event
        void mousePressEvent(QMouseEvent* );

        //! release event
        void mouseReleaseEvent(QMouseEvent* );

        //! enter event
        void enterEvent( QEvent* );

        //! leave event
        void leaveEvent(QEvent* );

        //! paint
        void paintEvent(QPaintEvent* );

        //! draw icon
        void drawIcon(QPainter*);

        //! color
        QColor buttonDetailColor(const QPalette& ) const;

        //! color
        QColor buttonDetailColor(const QPalette& palette, bool active ) const
        {
            return active ?
                palette.color(QPalette::Active, QPalette::WindowText):
                helper_.inactiveTitleBarTextColor( palette );
        }

        //! true if animation is in progress
        bool isAnimated( void ) const
        { return glowAnimation_->isRunning(); }

        //! true if button is active
        bool isActive( void ) const;

        //! true if buttons hover are animated
        bool animateButtonHover( void ) const;

        //!@name button properties
        //@{

        //! true if button if of menu type
        bool isMenuButton( void ) const
        { return type_ == ButtonMenu || type_ == ButtonItemMenu; }

        //! true if button is of toggle type
        bool isToggleButton( void ) const
        { return type_ == ButtonSticky || type_ == ButtonAbove || type_ == ButtonBelow; }

        //! true if button if of close type
        bool isCloseButton( void ) const
        { return type_ == ButtonClose || type_ == ButtonItemClose; }

        //! true if button has decoration
        bool hasDecoration( void ) const
        { return !isMenuButton() && type_ != ButtonItemClose; }

        //@}

        private:

        //! parent client
        const Client &client_;

        //! helper
        DecoHelper &helper_;

        //! button type
        ButtonType type_;

        //! button status
        ButtonState status_;

        //! true if button should be forced inactive
        bool forceInactive_;

        //! glow animation
        //Animation::Pointer glowAnimation_;
        Animation* glowAnimation_;

        //! glow intensity
        qreal glowIntensity_;


    };

} //namespace Oxygen

#endif
