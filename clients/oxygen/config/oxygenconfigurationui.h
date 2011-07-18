#ifndef oxygenconfigurationui_h
#define oxygenconfigurationui_h
//////////////////////////////////////////////////////////////////////////////
// oxygenconfigurationui.h
// -------------------
//
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

#include <kdeversion.h>
#include <KComboBox>
#include <QtGui/QWidget>
#include <QtGui/QCheckBox>
#include <QtCore/QVector>

#include "ui_oxygenconfigurationui.h"
#include "oxygenshadowconfigurationui.h"
#include "oxygenexceptionlistwidget.h"

namespace Oxygen
{

    class AnimationConfigWidget;

    //_____________________________________________
    class ConfigurationUi: public QWidget
    {

        Q_OBJECT

        public:

        //! constructor
        ConfigurationUi( QWidget* );

        //! destructor
        virtual ~ConfigurationUi( void )
        {}

        //! ui
        Ui_OxygenConfigurationUI ui;

        //! shadow configuration
        QVector<ShadowConfigurationUi*> shadowConfigurations;

        //! toggle expert mode
        void toggleExpertMode( bool );

        //! expert mode
        bool expertMode( void ) const
        { return _expertMode; }

        //! animation config widget
        AnimationConfigWidget* animationConfigWidget( void ) const
        { return _animationConfigWidget; }

        //! event filter
        virtual bool eventFilter( QObject*, QEvent* );

        protected Q_SLOTS:

        //! toggle expert mode
        void toggleExpertModeInternal( void )
        { toggleExpertModeInternal( !_expertMode ); }

        //! toggle expert mode
        void toggleExpertModeInternal( bool );

        //! update layout
        /*! needed in expert mode to accommodate with animations config widget size changes */
        void updateLayout( void );

        signals:

        //! emmited when changed
        void changed( void );

        private:

        //! expert mode
        bool _expertMode;

        //! animation config (expert mode only)
        AnimationConfigWidget* _animationConfigWidget;

    };

}

#endif
