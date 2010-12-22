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

#include "../oxygenconfiguration.h"
#include "../oxygenshadowconfiguration.h"

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

        configuration_ = new KConfig( "oxygenrc" );
        KConfigGroup configurationGroup( configuration_, "Windeco");

        userInterface_ = new ConfigurationUi( parent );

        load( configurationGroup );
        connect( userInterface_, SIGNAL(changed()), SLOT( updateChanged() ) );
        userInterface_->show();

    }


    //_______________________________________________________________________
    Config::~Config()
    {
        delete userInterface_;
        delete configuration_;
    }

    //_______________________________________________________________________
    void Config::toggleExpertMode( bool value )
    { userInterface_->toggleExpertMode( value ); }

    //_______________________________________________________________________
    void Config::load( const KConfigGroup& )
    {

        // load standard configuration
        loadConfiguration( Configuration( KConfigGroup( configuration_, "Windeco") ) );
        loadShadowConfiguration( QPalette::Active, ShadowConfiguration( QPalette::Active, KConfigGroup( configuration_, "ActiveShadow") ) );
        loadShadowConfiguration( QPalette::Inactive, ShadowConfiguration( QPalette::Inactive, KConfigGroup( configuration_, "InactiveShadow") ) );

        // load exceptions
        ExceptionList exceptions;
        exceptions.read( *configuration_ );
        if( exceptions.empty() )
        { exceptions = ExceptionList::defaultList(); }

        // install in ui
        userInterface_->ui.exceptions->setExceptions( exceptions );
        updateChanged();

    }

    //_______________________________________________________________________
    void Config::updateChanged( void )
    {

        Configuration configuration( KConfigGroup( configuration_, "Windeco") );
        bool modified( false );

        if( userInterface_->ui.titleAlignment->currentIndex() != userInterface_->ui.titleAlignment->findText( configuration.titleAlignmentName( true ) ) ) modified = true;
        else if( userInterface_->ui.buttonSize->currentIndex() != userInterface_->ui.buttonSize->findText( configuration.buttonSizeName( true ) ) ) modified = true;
        else if( userInterface_->ui.blendColor->currentIndex() != userInterface_->ui.blendColor->findText( configuration.blendColorName( true ) ) ) modified = true;
        else if( userInterface_->ui.frameBorder->currentIndex() != userInterface_->ui.frameBorder->findText( configuration.frameBorderName( true ) ) ) modified = true;
        else if( userInterface_->ui.sizeGripMode->currentIndex() != userInterface_->ui.sizeGripMode->findText( configuration.sizeGripModeName( true ) ) ) modified = true;
        else if( userInterface_->ui.shadowMode->currentIndex() != userInterface_->ui.shadowMode->findText( configuration.shadowModeName( true ) ) ) modified = true;
        else if( userInterface_->ui.shadowCacheMode->currentIndex() != userInterface_->ui.shadowCacheMode->findText( configuration.shadowCacheModeName( true ) ) ) modified = true;

        else if( userInterface_->ui.separatorMode->currentIndex() != configuration.separatorMode() ) modified = true;
        else if( userInterface_->ui.titleOutline->isChecked() !=  configuration.drawTitleOutline() ) modified = true;
        else if( userInterface_->ui.tabsEnabled->isChecked() !=  configuration.tabsEnabled() ) modified = true;
        else if( userInterface_->ui.useAnimations->isChecked() !=  configuration.useAnimations() ) modified = true;
        else if( userInterface_->ui.animateTitleChange->isChecked() !=  configuration.animateTitleChange() ) modified = true;
        else if( userInterface_->ui.narrowButtonSpacing->isChecked() !=  configuration.useNarrowButtonSpacing() ) modified = true;

        // shadow configurations
        else if( userInterface_->shadowConfigurations[0]->isChecked() !=  configuration.useOxygenShadows() ) modified = true;
        else if( userInterface_->shadowConfigurations[1]->isChecked() !=  configuration.useDropShadows() ) modified = true;
        else if( shadowConfigurationChanged( ShadowConfiguration( QPalette::Active, KConfigGroup( configuration_, "ActiveShadow") ), *userInterface_->shadowConfigurations[0] ) ) modified = true;
        else if( shadowConfigurationChanged( ShadowConfiguration( QPalette::Inactive, KConfigGroup( configuration_, "InactiveShadow") ), *userInterface_->shadowConfigurations[1] ) ) modified = true;

        // exceptions
        else if( exceptionListChanged() ) modified = true;

        // emit relevant signals
        if( modified ) emit changed();
        emit changed( modified );

    }

    //_______________________________________________________________________
    void Config::save( KConfigGroup& )
    {

        // create configuration from UI
        Configuration configuration;
        configuration.setTitleAlignment( Configuration::titleAlignment( userInterface_->ui.titleAlignment->currentText(), true ) );
        configuration.setButtonSize( Configuration::buttonSize( userInterface_->ui.buttonSize->currentText(), true ) );
        configuration.setBlendColor( Configuration::blendColor( userInterface_->ui.blendColor->currentText(), true ) );
        configuration.setFrameBorder( Configuration::frameBorder( userInterface_->ui.frameBorder->currentText(), true ) );
        configuration.setSizeGripMode( Configuration::sizeGripMode( userInterface_->ui.sizeGripMode->currentText(), true ) );
        configuration.setSeparatorMode( (Oxygen::Configuration::SeparatorMode) userInterface_->ui.separatorMode->currentIndex() );
        configuration.setDrawTitleOutline( userInterface_->ui.titleOutline->isChecked() );
        configuration.setUseDropShadows( userInterface_->shadowConfigurations[1]->isChecked() );
        configuration.setUseOxygenShadows( userInterface_->shadowConfigurations[0]->isChecked() );
        configuration.setTabsEnabled( userInterface_->ui.tabsEnabled->isChecked() );
        configuration.setUseAnimations( userInterface_->ui.useAnimations->isChecked() );
        configuration.setAnimateTitleChange( userInterface_->ui.animateTitleChange->isChecked() );
        configuration.setUseNarrowButtonSpacing( userInterface_->ui.narrowButtonSpacing->isChecked() );

        // save standard configuration
        KConfigGroup configurationGroup( configuration_, "Windeco");
        configurationGroup.deleteGroup();
        configuration.write( configurationGroup );

        // write exceptions
        userInterface_->ui.exceptions->exceptions().write( *configuration_ );

        // write shadow configuration
        configurationGroup.writeEntry( OxygenConfig::SHADOW_MODE,
            Configuration::shadowModeName( Configuration::shadowMode( userInterface_->ui.shadowMode->currentText(), true ), false ) );
        saveShadowConfiguration( QPalette::Active, *userInterface_->shadowConfigurations[0] );
        saveShadowConfiguration( QPalette::Inactive, *userInterface_->shadowConfigurations[1] );

        // sync configuration
        configuration_->sync();

    }


    //_______________________________________________________________________
    void Config::saveShadowConfiguration( QPalette::ColorGroup colorGroup, const ShadowConfigurationUi& ui ) const
    {

        assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );

        // create shadow configuration
        ShadowConfiguration configuration( colorGroup );
        configuration.setShadowSize( ui.ui.shadowSize->value() );
        configuration.setHorizontalOffset( 0.1*ui.ui.verticalOffset->value() );
        configuration.setInnerColor( ui.ui.innerColor->color() );
        configuration.setOuterColor( ui.ui.outerColor->color() );
        configuration.setUseOuterColor( ui.ui.useOuterColor->isChecked() );

        // save shadow configuration
        KConfigGroup configurationGroup( configuration_, ( (colorGroup == QPalette::Active) ? "ActiveShadow":"InactiveShadow" ) );
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
        userInterface_->ui.exceptions->setExceptions( ExceptionList::defaultList() );

        updateChanged();

    }

    //_______________________________________________________________________
    void Config::loadConfiguration( const Configuration& configuration )
    {

        userInterface_->ui.titleAlignment->setCurrentIndex( userInterface_->ui.titleAlignment->findText( configuration.titleAlignmentName( true ) ) );
        userInterface_->ui.buttonSize->setCurrentIndex( userInterface_->ui.buttonSize->findText( configuration.buttonSizeName( true ) ) );
        userInterface_->ui.blendColor->setCurrentIndex( userInterface_->ui.blendColor->findText( configuration.blendColorName( true ) ) );
        userInterface_->ui.frameBorder->setCurrentIndex( userInterface_->ui.frameBorder->findText( configuration.frameBorderName( true ) ) );
        userInterface_->ui.sizeGripMode->setCurrentIndex( userInterface_->ui.sizeGripMode->findText( configuration.sizeGripModeName( true ) ) );

        userInterface_->ui.separatorMode->setCurrentIndex( configuration.separatorMode() );
        userInterface_->ui.titleOutline->setChecked( configuration.drawTitleOutline() );
        userInterface_->shadowConfigurations[0]->setChecked( configuration.useOxygenShadows() );
        userInterface_->shadowConfigurations[1]->setChecked( configuration.useDropShadows() );
        userInterface_->ui.tabsEnabled->setChecked( configuration.tabsEnabled() );
        userInterface_->ui.useAnimations->setChecked( configuration.useAnimations() );
        userInterface_->ui.animateTitleChange->setChecked( configuration.animateTitleChange() );
        userInterface_->ui.narrowButtonSpacing->setChecked( configuration.useNarrowButtonSpacing() );
        userInterface_->ui.shadowMode->setCurrentIndex( userInterface_->ui.shadowMode->findText( configuration.shadowModeName( true ) ) );
        userInterface_->ui.shadowCacheMode->setCurrentIndex( userInterface_->ui.shadowCacheMode->findText( configuration.shadowCacheModeName( true ) ) );
    }

    //_______________________________________________________________________
    void Config::loadShadowConfiguration( QPalette::ColorGroup colorGroup, const ShadowConfiguration& configuration )
    {
        assert( colorGroup == QPalette::Active || colorGroup == QPalette::Inactive );
        ShadowConfigurationUi* ui = userInterface_->shadowConfigurations[ (colorGroup == QPalette::Active) ? 0:1 ];
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
        exceptions.read( *configuration_ );
        if( exceptions.empty() )
        { exceptions = ExceptionList::defaultList(); }

        // compare to current
        return exceptions != userInterface_->ui.exceptions->exceptions();

    }

}
