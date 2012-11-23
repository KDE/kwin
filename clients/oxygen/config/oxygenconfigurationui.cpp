//////////////////////////////////////////////////////////////////////////////
// oxygenconfigurationui.cpp
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

#include "oxygenconfigurationui.h"
#include "oxygenanimationconfigwidget.h"

#include <kdeversion.h>

#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QGroupBox>
#include <KLocale>
#include <KTabWidget>

namespace Oxygen
{

    //_________________________________________________________
    ConfigurationUi::ConfigurationUi( QWidget* parent ):
        QWidget( parent ),
        _expertMode( false ),
        _animationConfigWidget(0)
    {

        ui.setupUi( this );

        // shadow configuration
        ui.activeShadowConfiguration->setGroup( QPalette::Active );
        ui.inactiveShadowConfiguration->setGroup( QPalette::Inactive );

        shadowConfigurations.append( ui.activeShadowConfiguration );
        shadowConfigurations.append( ui.inactiveShadowConfiguration );

        // connections
        connect( ui.titleOutline, SIGNAL(toggled(bool)), ui.separatorMode, SLOT(setDisabled(bool)) );

        connect( shadowConfigurations[0], SIGNAL(changed()), SIGNAL(changed()) );
        connect( shadowConfigurations[0], SIGNAL(toggled(bool)), SIGNAL(changed()) );

        connect( shadowConfigurations[1], SIGNAL(changed()), SIGNAL(changed()) );
        connect( shadowConfigurations[1], SIGNAL(toggled(bool)), SIGNAL(changed()) );

        connect( ui.titleAlignment, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
        connect( ui.buttonSize, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
        connect( ui.frameBorder, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
        connect( ui.blendColor, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );

        connect( ui.titleOutline, SIGNAL(clicked()), SIGNAL(changed()) );
        connect( ui.drawSizeGrip, SIGNAL(clicked()), SIGNAL(changed()) );
        connect( ui.narrowButtonSpacing, SIGNAL(clicked()), SIGNAL(changed()) );
        connect( ui.separatorMode, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
        connect( ui.exceptions, SIGNAL(changed()), SIGNAL(changed()) );

        connect( ui._expertModeButton, SIGNAL(pressed()), SLOT(toggleExpertModeInternal()) );

        ui._expertModeButton->setIcon( KIcon("configure") );

        // animation config widget
        connect( ui.animationsEnabled, SIGNAL(clicked()), SIGNAL(changed()) );
        _animationConfigWidget = new AnimationConfigWidget();
        _animationConfigWidget->installEventFilter( this );
        connect( _animationConfigWidget, SIGNAL(changed(bool)), SIGNAL(changed()) );
        connect( _animationConfigWidget, SIGNAL(layoutChanged()), SLOT(updateLayout()) );

        toggleExpertModeInternal( false );

    }

    //_________________________________________________________
    void ConfigurationUi::toggleExpertMode( bool value )
    {
        ui._expertModeContainer->hide();
        toggleExpertModeInternal( value );
    }

    //_________________________________________________________
    void ConfigurationUi::toggleExpertModeInternal( bool value )
    {

        // store value
        _expertMode = value;

        // update button text
        ui._expertModeButton->setText( _expertMode ? i18n( "Hide Advanced Configuration Options" ):i18n( "Show Advanced Configuration Options" ) );

        // narrow button spacing
        ui.narrowButtonSpacing->setVisible( _expertMode );

        // size grip
        ui.drawSizeGrip->setVisible( _expertMode );

        // 'basic' animations enabled flag
        ui.animationsEnabled->setVisible( !_expertMode );

        // layout and animations
        if( _expertMode )
        {

            // add animationConfigWidget to tabbar if needed
            if( ui.tabWidget->indexOf( _animationConfigWidget ) < 0 )
            { ui.tabWidget->insertTab( 1, _animationConfigWidget, i18n( "Animations" ) ); }

            ui.shadowSpacer->changeSize(0,0, QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

        } else {

            ui.shadowSpacer->changeSize(0,0, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

            if( int index = ui.tabWidget->indexOf( _animationConfigWidget ) >= 0 )
            { ui.tabWidget->removeTab( index ); }

        }

    }

    //__________________________________________________________________
    bool ConfigurationUi::eventFilter( QObject* object, QEvent* event )
    {

        switch( event->type() )
        {
            case QEvent::ShowToParent:
            object->event( event );
            updateLayout();
            return true;

            default:
            return false;
        }
    }

    //__________________________________________________________________
    void ConfigurationUi::updateLayout( void )
    {

        int delta = _animationConfigWidget->minimumSizeHint().height() - _animationConfigWidget->size().height();
        window()->setMinimumSize( QSize( window()->minimumSizeHint().width(), window()->size().height() + delta ) );

    }

}
