//////////////////////////////////////////////////////////////////////////////
// config.cpp
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>
//
// Based on the Quartz configuration module,
//     Copyright (c) 2001 Karol Szwed <gallium@kde.org>
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

#include "oxygenconfig.h"
#include "oxygenconfig.moc"

#include "oxygenanimationconfigwidget.h"
#include "oxygenshadowconfiguration.h"
#include "../oxygenconfiguration.h"

#include <QtCore/QTextStream>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include <KAboutData>
#include <KAboutApplicationDialog>
#include <KConfigGroup>
#include <KGlobal>
#include <KLocale>

//_______________________________________________________________________
extern "C"
{
    KDE_EXPORT QObject* allocate_config( KConfig* conf, QWidget* parent )
    { return ( new Oxygen::Config( conf, parent ) ); }
}

namespace Oxygen
{

    //_______________________________________________________________________
    Config::Config( KConfig*, QWidget* parent ):
        QObject( parent )
    {

        KGlobal::locale()->insertCatalog("kwin_clients");

        _configuration = new KConfig( "oxygenrc" );
        KConfigGroup configurationGroup( _configuration, "Windeco");

        ui = new ConfigurationUi( parent );

        load( configurationGroup );
        connect( ui, SIGNAL(changed()), SLOT(updateChanged()) );
        ui->show();

    }

    //_______________________________________________________________________
    Config::~Config()
    {
        delete ui;
        delete _configuration;
    }

    //_______________________________________________________________________
    void Config::toggleExpertMode( bool value )
    { ui->toggleExpertMode( value ); }

    //_______________________________________________________________________
    void Config::load( const KConfigGroup& )
    {

        // load standard configuration
        loadConfiguration( Configuration( KConfigGroup( _configuration, "Windeco") ) );
        loadShadowConfiguration( QPalette::Active, ShadowConfiguration( QPalette::Active, KConfigGroup( _configuration, "ActiveShadow") ) );
        loadShadowConfiguration( QPalette::Inactive, ShadowConfiguration( QPalette::Inactive, KConfigGroup( _configuration, "InactiveShadow") ) );

        // load exceptions
        ExceptionList exceptions;
        exceptions.read( *_configuration );
        if( exceptions.empty() )
        { exceptions = ExceptionList::defaultList(); }

        // install in ui
        ui->ui.exceptions->setExceptions( exceptions );
        updateChanged();

    }

    //_______________________________________________________________________
    void Config::updateChanged( void )
    {

        Configuration configuration( KConfigGroup( _configuration, "Windeco") );
        bool modified( false );

        if( ui->ui.titleAlignment->currentIndex() != ui->ui.titleAlignment->findText( configuration.titleAlignmentName( true ) ) ) modified = true;
        else if( ui->ui.buttonSize->currentIndex() != ui->ui.buttonSize->findText( configuration.buttonSizeName( true ) ) ) modified = true;
        else if( ui->ui.blendColor->currentIndex() != ui->ui.blendColor->findText( configuration.blendColorName( true ) ) ) modified = true;
        else if( ui->ui.frameBorder->currentIndex() != ui->ui.frameBorder->findText( configuration.frameBorderName( true ) ) ) modified = true;
        else if( ui->ui.sizeGripMode->currentIndex() != ui->ui.sizeGripMode->findText( configuration.sizeGripModeName( true ) ) ) modified = true;

        else if( ui->ui.separatorMode->currentIndex() != configuration.separatorMode() ) modified = true;
        else if( ui->ui.titleOutline->isChecked() !=  configuration.drawTitleOutline() ) modified = true;
        else if( ui->ui.narrowButtonSpacing->isChecked() !=  configuration.useNarrowButtonSpacing() ) modified = true;

        // shadow configurations
        else if( ui->shadowConfigurations[0]->isChecked() !=  configuration.useOxygenShadows() ) modified = true;
        else if( ui->shadowConfigurations[1]->isChecked() !=  configuration.useDropShadows() ) modified = true;
        else if( shadowConfigurationChanged( ShadowConfiguration( QPalette::Active, KConfigGroup( _configuration, "ActiveShadow") ), *ui->shadowConfigurations[0] ) ) modified = true;
        else if( shadowConfigurationChanged( ShadowConfiguration( QPalette::Inactive, KConfigGroup( _configuration, "InactiveShadow") ), *ui->shadowConfigurations[1] ) ) modified = true;

        // exceptions
        else if( exceptionListChanged() ) modified = true;

        // animations
        else if( !ui->expertMode() && ui->ui.animationsEnabled->isChecked() !=  configuration.animationsEnabled() ) modified = true;
        else if( ui->expertMode() && ui->animationConfigWidget()->isChanged() ) modified = true;

        // emit relevant signals
        if( modified ) emit changed();
        emit changed( modified );

    }

    //_______________________________________________________________________
    void Config::save( KConfigGroup& )
    {

        // create configuration from UI
        Configuration configuration;
        configuration.setTitleAlignment( Configuration::titleAlignment( ui->ui.titleAlignment->currentText(), true ) );
        configuration.setCenterTitleOnFullWidth( ui->ui.titleAlignment->currentText() == Configuration::titleAlignmentName( Qt::AlignHCenter, true, true ) );
        configuration.setButtonSize( Configuration::buttonSize( ui->ui.buttonSize->currentText(), true ) );
        configuration.setBlendColor( Configuration::blendColor( ui->ui.blendColor->currentText(), true ) );
        configuration.setFrameBorder( Configuration::frameBorder( ui->ui.frameBorder->currentText(), true ) );
        configuration.setSizeGripMode( Configuration::sizeGripMode( ui->ui.sizeGripMode->currentText(), true ) );
        configuration.setSeparatorMode( (Oxygen::Configuration::SeparatorMode) ui->ui.separatorMode->currentIndex() );
        configuration.setDrawTitleOutline( ui->ui.titleOutline->isChecked() );
        configuration.setUseDropShadows( ui->shadowConfigurations[1]->isChecked() );
        configuration.setUseOxygenShadows( ui->shadowConfigurations[0]->isChecked() );
        configuration.setUseNarrowButtonSpacing( ui->ui.narrowButtonSpacing->isChecked() );

        if( ui->expertMode() )
        {

            ui->animationConfigWidget()->setConfiguration( configuration );
            ui->animationConfigWidget()->save();
            configuration = ui->animationConfigWidget()->configuration();

        } else {

            configuration.setAnimationsEnabled( ui->ui.animationsEnabled->isChecked() );

        }

        // save standard configuration
        KConfigGroup configurationGroup( _configuration, "Windeco");
        configurationGroup.deleteGroup();
        configuration.write( configurationGroup );

        // write exceptions
        ui->ui.exceptions->exceptions().write( *_configuration );

        // write shadow configuration
        saveShadowConfiguration( QPalette::Active, *ui->shadowConfigurations[0] );
        saveShadowConfiguration( QPalette::Inactive, *ui->shadowConfigurations[1] );

        // sync configuration
        _configuration->sync();

        QDBusMessage message( QDBusMessage::createSignal("/OxygenWindeco",  "org.kde.Oxygen.Style", "reparseConfiguration") );
        QDBusConnection::sessionBus().send(message);

    }

    //_______________________________________________________________________
    void Config::saveShadowConfiguration( QPalette::ColorGroup colorGroup, const ShadowConfigurationUi& ui ) const
    {

        assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );

        // create shadow configuration
        ShadowConfiguration configuration( colorGroup );
        configuration.setShadowSize( ui.ui.shadowSize->value() );
        configuration.setVerticalOffset( 0.1*ui.ui.verticalOffset->value() );
        configuration.setInnerColor( ui.ui.innerColor->color() );
        configuration.setOuterColor( ui.ui.outerColor->color() );
        configuration.setUseOuterColor( ui.ui.useOuterColor->isChecked() );

        // save shadow configuration
        KConfigGroup configurationGroup( _configuration, ( (colorGroup == QPalette::Active) ? "ActiveShadow":"InactiveShadow" ) );
        configurationGroup.deleteGroup();
        configuration.write( configurationGroup );

    }

    //_______________________________________________________________________
    void Config::defaults()
    {

        // install default configuration
        loadConfiguration( Configuration() );

        // load shadows
        loadShadowConfiguration( QPalette::Active, ShadowConfiguration( QPalette::Active ) );
        loadShadowConfiguration( QPalette::Inactive, ShadowConfiguration( QPalette::Inactive ) );

        // install default exceptions
        ui->ui.exceptions->setExceptions( ExceptionList::defaultList() );

        updateChanged();

    }

    //_______________________________________________________________________
    void Config::loadConfiguration( const Configuration& configuration )
    {

        ui->ui.titleAlignment->setCurrentIndex( ui->ui.titleAlignment->findText( configuration.titleAlignmentName( true ) ) );
        ui->ui.buttonSize->setCurrentIndex( ui->ui.buttonSize->findText( configuration.buttonSizeName( true ) ) );
        ui->ui.blendColor->setCurrentIndex( ui->ui.blendColor->findText( configuration.blendColorName( true ) ) );
        ui->ui.frameBorder->setCurrentIndex( ui->ui.frameBorder->findText( configuration.frameBorderName( true ) ) );
        ui->ui.sizeGripMode->setCurrentIndex( ui->ui.sizeGripMode->findText( configuration.sizeGripModeName( true ) ) );

        ui->ui.separatorMode->setCurrentIndex( configuration.separatorMode() );
        ui->ui.titleOutline->setChecked( configuration.drawTitleOutline() );
        ui->shadowConfigurations[0]->setChecked( configuration.useOxygenShadows() );
        ui->shadowConfigurations[1]->setChecked( configuration.useDropShadows() );
        ui->ui.animationsEnabled->setChecked( configuration.animationsEnabled() );
        ui->ui.narrowButtonSpacing->setChecked( configuration.useNarrowButtonSpacing() );

        ui->animationConfigWidget()->setConfiguration( configuration );
        ui->animationConfigWidget()->load();

    }

    //_______________________________________________________________________
    void Config::loadShadowConfiguration( QPalette::ColorGroup colorGroup, const ShadowConfiguration& configuration )
    {
        assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );
        ShadowConfigurationUi* ui = this->ui->shadowConfigurations[ (colorGroup == QPalette::Active) ? 0:1 ];
        ui->ui.shadowSize->setValue( configuration.shadowSize() );
        ui->ui.verticalOffset->setValue( 10*configuration.verticalOffset() );
        ui->ui.innerColor->setColor( configuration.innerColor() );
        ui->ui.outerColor->setColor( configuration.outerColor() );
        ui->ui.useOuterColor->setChecked( configuration.useOuterColor() );
    }

    //_______________________________________________________________________
    bool Config::shadowConfigurationChanged( const ShadowConfiguration& configuration, const ShadowConfigurationUi& ui ) const
    {
        bool modified( false );

        if( ui.ui.shadowSize->value() != configuration.shadowSize() ) modified = true;
        else if( 0.1*ui.ui.verticalOffset->value() != configuration.verticalOffset() ) modified = true;
        else if( ui.ui.innerColor->color() != configuration.innerColor() ) modified = true;
        else if( ui.ui.useOuterColor->isChecked() != configuration.useOuterColor() ) modified = true;
        else if( ui.ui.useOuterColor->isChecked() && ui.ui.outerColor->color() != configuration.outerColor() ) modified = true;
        return modified;
    }

    //_______________________________________________________________________
    bool Config::exceptionListChanged( void ) const
    {

        // get saved list
        ExceptionList exceptions;
        exceptions.read( *_configuration );
        if( exceptions.empty() )
        { exceptions = ExceptionList::defaultList(); }

        // compare to current
        return exceptions != ui->ui.exceptions->exceptions();

    }

}
