//////////////////////////////////////////////////////////////////////////////
// oxygenanimationconfigwidget.cpp
// animation configuration widget
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

#include "oxygenanimationconfigwidget.h"
#include "oxygenanimationconfigwidget.moc"
#include "oxygenanimationconfigitem.h"
#include "oxygengenericanimationconfigitem.h"

#include <QtGui/QButtonGroup>
#include <QtGui/QHoverEvent>
#include <QtCore/QTextStream>
#include <KLocale>

namespace Oxygen
{

    //_______________________________________________
    AnimationConfigWidget::AnimationConfigWidget( QWidget* parent ):
        BaseAnimationConfigWidget( parent )
    {

        QGridLayout* layout( qobject_cast<QGridLayout*>( BaseAnimationConfigWidget::layout() ) );

        setupItem( layout, _buttonAnimations = new GenericAnimationConfigItem( this,
            i18n("Button mouseover transition"),
            i18n("Configure window buttons' mouseover highlight animation" ) ) );

        setupItem( layout, _titleAnimations = new GenericAnimationConfigItem( this,
            i18n("Title transitions" ),
            i18n("Configure fading transitions when window title is changed" ) ) );

        setupItem( layout, _shadowAnimations = new GenericAnimationConfigItem( this,
            i18n("Window active state change transitions" ),
            i18n("Configure fading between window shadow and glow when window's active state is changed" ) ) );

        setupItem( layout, _tabAnimations = new GenericAnimationConfigItem( this,
            i18n("Window grouping animations" ),
            i18n("Configure window titlebar animations when windows are grouped/ungrouped" ) ) );

        // add spacers to the first column, previous row to finalize layout
        layout->addItem( new QSpacerItem( 25, 0 ), _row-1, 0, 1, 1 );

        // add vertical spacer
        layout->addItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding ), _row, 1, 1, 1 );
        ++_row;

        connect( animationsEnabled(), SIGNAL(toggled(bool)), SLOT(updateChanged()) );
        foreach( AnimationConfigItem* item, findChildren<AnimationConfigItem*>() )
        {
            item->QWidget::setEnabled( false );
            connect( animationsEnabled(), SIGNAL(toggled(bool)), item, SLOT(setEnabled(bool)) );
        }

    }

    //_______________________________________________
    AnimationConfigWidget::~AnimationConfigWidget( void )
    {}

    //_______________________________________________
    void AnimationConfigWidget::load( void )
    {

        animationsEnabled()->setChecked( _configuration.animationsEnabled() );

        _buttonAnimations->setEnabled( _configuration.buttonAnimationsEnabled() );
        _buttonAnimations->setDuration( _configuration.buttonAnimationsDuration() );

        _titleAnimations->setEnabled( _configuration.titleAnimationsEnabled() );
        _titleAnimations->setDuration( _configuration.titleAnimationsDuration() );

        _shadowAnimations->setEnabled( _configuration.shadowAnimationsEnabled() );
        _shadowAnimations->setDuration( _configuration.shadowAnimationsDuration() );

        _tabAnimations->setEnabled( _configuration.tabAnimationsEnabled() );
        _tabAnimations->setDuration( _configuration.tabAnimationsDuration() );
    }

    //_______________________________________________
    void AnimationConfigWidget::save( void )
    {

        _configuration.setAnimationsEnabled( animationsEnabled()->isChecked() );

        _configuration.setButtonAnimationsEnabled( _buttonAnimations->enabled() );
        _configuration.setButtonAnimationsDuration( _buttonAnimations->duration() );

        _configuration.setTitleAnimationsEnabled( _titleAnimations->enabled() );
        _configuration.setTitleAnimationsDuration( _titleAnimations->duration() );

        _configuration.setShadowAnimationsEnabled( _shadowAnimations->enabled() );
        _configuration.setShadowAnimationsDuration( _shadowAnimations->duration() );

        _configuration.setTabAnimationsEnabled( _tabAnimations->enabled() );
        _configuration.setTabAnimationsDuration( _tabAnimations->duration() );

        setChanged( false );

    }

    //_______________________________________________
    void AnimationConfigWidget::updateChanged( void )
    {

        bool modified( false );

        if( animationsEnabled()->isChecked() != _configuration.animationsEnabled() ) modified = true;
        else if( _buttonAnimations->enabled() != _configuration.buttonAnimationsEnabled() ) modified = true;
        else if( _buttonAnimations->duration() != _configuration.buttonAnimationsDuration() ) modified = true;

        else if( _titleAnimations->enabled() != _configuration.titleAnimationsEnabled() ) modified = true;
        else if( _titleAnimations->duration() != _configuration.titleAnimationsDuration() ) modified = true;

        else if( _shadowAnimations->enabled() != _configuration.shadowAnimationsEnabled() ) modified = true;
        else if( _shadowAnimations->duration() != _configuration.shadowAnimationsDuration() ) modified = true;

        else if( _tabAnimations->enabled() != _configuration.tabAnimationsEnabled() ) modified = true;
        else if( _tabAnimations->duration() != _configuration.tabAnimationsDuration() ) modified = true;

        setChanged( modified );

    }

}
