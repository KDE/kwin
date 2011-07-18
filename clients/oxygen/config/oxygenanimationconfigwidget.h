#ifndef oxygenanimationconfigwidget_h
#define oxygenanimationconfigwidget_h

//////////////////////////////////////////////////////////////////////////////
// oxygenanimationconfigwidget.h
// animation configuration item
// -------------------
//
// Copyright (c) 2010 Hugo Pereira Da Costa <hugo@oxygen-icons.org>
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

#include "oxygenbaseanimationconfigwidget.h"
#include "../oxygenconfiguration.h"

namespace Oxygen
{

    // forward declaration
    class GenericAnimationConfigItem;

    //! container to configure animations individually
    class AnimationConfigWidget: public BaseAnimationConfigWidget
    {

        Q_OBJECT

        public:

        //! constructor
        explicit AnimationConfigWidget( QWidget* = 0 );

        //! destructor
        virtual ~AnimationConfigWidget( void );

        //! configuration
        void setConfiguration( const Configuration& configuration )
        { _configuration = configuration; }

        //! configuration
        const Configuration& configuration( void )  const
        { return _configuration; }

        public slots:

        //! read current configuration
        virtual void load( void );

        //! save current configuration
        virtual void save( void );

        protected slots:

        //! check whether configuration is changed and emit appropriate signal if yes
        virtual void updateChanged();

        private:

        //! local configuration, needed to handle I/O
        Configuration _configuration;

        //!@name animations
        //@{
        GenericAnimationConfigItem* _buttonAnimations;
        GenericAnimationConfigItem* _titleAnimations;
        GenericAnimationConfigItem* _shadowAnimations;
        GenericAnimationConfigItem* _tabAnimations;
        //@}

    };

}

#endif
