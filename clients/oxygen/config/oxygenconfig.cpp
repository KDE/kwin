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
#include "oxygenconfiguration.h"
#include "oxygenutil.h"
#include "../oxygenexceptionlist.h"

#include <QTextStream>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include <KConfigGroup>
#include <KLocalizedString>

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

        // catalog
        KLocalizedString::insertCatalog( QStringLiteral( "kwin_clients" ) );

        // configuration
        _configuration = KSharedConfig::openConfig( QStringLiteral( "oxygenrc" ) );

        _configWidget = new ConfigWidget( parent );

        load();
        connect( _configWidget, SIGNAL(changed(bool)), SLOT(updateChanged()) );
        _configWidget->show();

    }

    //_______________________________________________________________________
    Config::~Config()
    { delete _configWidget; }

    //_______________________________________________________________________
    void Config::toggleExpertMode( bool value )
    { _configWidget->toggleExpertMode( value ); }

    //_______________________________________________________________________
    void Config::load( void )
    {

        // load standard configuration
        ConfigurationPtr configuration( new Configuration() );
        configuration->readConfig();
        loadConfiguration( configuration );

        // load shadows
        foreach( ShadowConfigWidget* ui, _configWidget->shadowConfigurations )
        { ui->readConfig( _configuration.data() ); }

        // load exceptions
        ExceptionList exceptions;
        exceptions.readConfig( _configuration );
        _configWidget->exceptionListWidget()->setExceptions( exceptions.get() );
        updateChanged();

    }

    //_______________________________________________________________________
    void Config::updateChanged( void )
    {

        ConfigurationPtr configuration( new Configuration() );
        configuration->readConfig();
        bool modified( false );

        // exceptions
        if( _configWidget->isChanged() ) modified = true;

        // emit relevant signals
        if( modified ) emit changed();
        emit changed( modified );

    }

    //_______________________________________________________________________
    void Config::save( void )
    {

        // create configuration from group
        ConfigurationPtr configuration( new Configuration() );
        configuration->readConfig();

        // save config widget
        _configWidget->setConfiguration( configuration );
        _configWidget->save();

        // save standard configuration
        Util::writeConfig( configuration.data(), _configuration.data() );

        // get list of exceptions and write
        ConfigurationList exceptions( _configWidget->exceptionListWidget()->exceptions() );
        ExceptionList( exceptions ).writeConfig( _configuration );

        // write shadow configuration
        foreach( ShadowConfigWidget* ui, _configWidget->shadowConfigurations )
        { ui->writeConfig( _configuration.data() ); }

        // sync configuration
        _configuration->sync();

        QDBusMessage message( QDBusMessage::createSignal( QStringLiteral( "/OxygenWindeco" ),  QStringLiteral( "org.kde.Oxygen.Style" ), QStringLiteral( "reparseConfiguration") ) );
        QDBusConnection::sessionBus().send(message);

    }

    //_______________________________________________________________________
    void Config::defaults( void )
    {

        // install default configuration
        ConfigurationPtr configuration( new Configuration() );
        configuration->setDefaults();
        loadConfiguration( configuration );

        // load shadows
        foreach( ShadowConfigWidget* ui, _configWidget->shadowConfigurations )
        { ui->readDefaults( _configuration.data() ); }

        updateChanged();

    }

    //_______________________________________________________________________
    void Config::loadConfiguration( ConfigurationPtr configuration )
    {

        _configWidget->setConfiguration( configuration );
        _configWidget->load();

    }

}
