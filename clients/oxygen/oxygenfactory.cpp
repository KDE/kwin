//////////////////////////////////////////////////////////////////////////////
// oxygen.cpp
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <riccardo@kde.org>
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

#include "oxygenfactory.h"
#include "oxygenfactory.moc"
#include "oxygenclient.h"
#include "oxygenexceptionlist.h"

#include <KSharedConfig>
#include <KConfigGroup>
#include <KWindowInfo>
#include <kdeversion.h>

KWIN_DECORATION(Oxygen::Factory)

namespace Oxygen
{

    //___________________________________________________
    Factory::Factory():
        _initialized( false ),
        _helper(),
        _shadowCache( _helper )
    {
        readConfig();
        setInitialized( true );
    }

    //___________________________________________________
    Factory::~Factory()
    { setInitialized( false ); }

    //___________________________________________________
    KDecoration* Factory::createDecoration(KDecorationBridge* bridge )
    { return (new Client( bridge, this ))->decoration(); }

    //___________________________________________________
    bool Factory::reset(unsigned long changed)
    {

        if( changed & SettingColors )
        { _shadowCache.invalidateCaches(); }

        // read in the configuration
        setInitialized( false );
        readConfig();
        setInitialized( true );
        return true;

    }

    //___________________________________________________
    void Factory::readConfig()
    {

        /*
        always reload helper
        this is needed to properly handle
        color contrast settings changed
        */
        helper().invalidateCaches();
        helper().reloadConfig();

        // initialize default configuration and read
        if( !_defaultConfiguration ) _defaultConfiguration = ConfigurationPtr(new Configuration());
        _defaultConfiguration->setCurrentGroup( QStringLiteral("Windeco") );
        _defaultConfiguration->readConfig();

        // create a config object
        KSharedConfig::Ptr config( KSharedConfig::openConfig( QStringLiteral("oxygenrc") ) );

        // clear exceptions and read
        ExceptionList exceptions;
        exceptions.readConfig( config );
        _exceptions = exceptions.get();

        // read shadowCache configuration
        _shadowCache.readConfig();
        _shadowCache.setAnimationsDuration( _defaultConfiguration->shadowAnimationsDuration() );

        // background pixmap
        {
            KConfigGroup group( config->group("Common") );
            helper().setBackgroundPixmap( group.readEntry( "BackgroundPixmap", "" ) );
        }

    }

    //_________________________________________________________________
    bool Factory::supports( Ability ability ) const
    {
        switch( ability )
        {

            // announce
            case AbilityAnnounceButtons:
            case AbilityAnnounceColors:

            // buttons
            case AbilityButtonMenu:
            case AbilityButtonApplicationMenu:
            case AbilityButtonHelp:
            case AbilityButtonMinimize:
            case AbilityButtonMaximize:
            case AbilityButtonClose:
            case AbilityButtonOnAllDesktops:
            case AbilityButtonAboveOthers:
            case AbilityButtonBelowOthers:
            case AbilityButtonSpacer:
            case AbilityButtonShade:

            // compositing
            case AbilityProvidesShadow:
            return true;

            case AbilityUsesAlphaChannel:
            case AbilityAnnounceAlphaChannel:
            return true;

            // tabs
            case AbilityTabbing:
            return true;

            // no colors supported at this time
            default:
            return false;
        };
    }



    //____________________________________________________________________
    Factory::ConfigurationPtr Factory::configuration( const Client& client )
    {

        QString windowTitle;
        QString className;
        foreach( const ConfigurationPtr& configuration, _exceptions )
        {

            // discard disabled exceptions
            if( !configuration->enabled() ) continue;

            // discard exceptions with empty exception pattern
            if( configuration->exceptionPattern().isEmpty() ) continue;

            /*
            decide which value is to be compared
            to the regular expression, based on exception type
            */
            QString value;
            switch( configuration->exceptionType() )
            {
                case Configuration::ExceptionWindowTitle:
                {
                    value = windowTitle.isEmpty() ? (windowTitle = client.caption()):windowTitle;
                    break;
                }

                default:
                case Configuration::ExceptionWindowClassName:
                {
                    if( className.isEmpty() )
                    {
                        // retrieve class name
                        KWindowInfo info( client.windowId(), 0, NET::WM2WindowClass );
                        QString window_className( QString::fromUtf8(info.windowClassName()) );
                        QString window_class( QString::fromUtf8(info.windowClassClass()) );
                        className = window_className + QStringLiteral(" ") + window_class;
                    }

                    value = className;
                    break;
                }

            }

            // check matching
            if( QRegExp( configuration->exceptionPattern() ).indexIn( value ) >= 0 )
            { return configuration; }

        }

        return _defaultConfiguration;

    }

}
