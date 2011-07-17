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

#include <cassert>
#include <KConfigGroup>
#include <KDebug>
#include <KGlobal>
#include <KWindowInfo>
#include <kdeversion.h>

extern "C"
{
    KDE_EXPORT KDecorationFactory* create_factory()
    { return new Oxygen::Factory(); }
}

namespace Oxygen
{

    //___________________________________________________
    Factory::Factory():
        _initialized( false ),
        _helper( "oxygenDeco" ),
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
        { shadowCache().invalidateCaches(); }

        // read in the configuration
        setInitialized( false );
        bool _configurationchanged = readConfig();
        setInitialized( true );

        if( _configurationchanged || (changed & (SettingDecoration | SettingButtons | SettingBorder)) )
        {

            // returning true triggers all decorations to be re-created
            return true;

        } else {

            // no need to re-create the decorations
            // trigger repaint only
            resetDecorations(changed);
            return false;

        }

    }

    //___________________________________________________
    bool Factory::readConfig()
    {

        bool changed( false );

        /*
        always reload helper
        this is needed to properly handle
        color contrast settings changed
        */
        helper().invalidateCaches();
        helper().reloadConfig();

        // create a config object
        KConfig config("oxygenrc");
        KConfigGroup group( config.group("Windeco") );
        Configuration configuration( group );
        if( !( configuration == defaultConfiguration() ) )
        {
            setDefaultConfiguration( configuration );
            changed = true;
        }

        // read exceptionsreadConfig
        ExceptionList exceptions( config );
        if( !( exceptions == _exceptions ) )
        {
            _exceptions = exceptions;
            changed = true;
        }

        // read shadowCache configuration
        changed |= shadowCache().readConfig( config );

        // background pixmap
        {
            KConfigGroup group( config.group("Common") );
            helper().setBackgroundPixmap( group.readEntry( "BackgroundPixmap", "" ) );
        }

        return changed;

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
            case AbilityButtonHelp:
            case AbilityButtonMinimize:
            case AbilityButtonMaximize:
            case AbilityButtonClose:
            case AbilityButtonOnAllDesktops:
            case AbilityButtonAboveOthers:
            case AbilityButtonBelowOthers:
            case AbilityButtonSpacer:
            case AbilityButtonShade:

            //       // colors
            //       case AbilityColorTitleBack:
            //       case AbilityColorTitleFore:
            //       case AbilityColorFrame:

            // compositing
            case AbilityProvidesShadow: // TODO: UI option to use default shadows instead
            return true;

            case AbilityUsesAlphaChannel:
            return true;

            // tabs
            case AbilityClientGrouping:
            return true;

            // no colors supported at this time
            default:
            return false;
        };
    }



    //____________________________________________________________________
    Configuration Factory::configuration( const Client& client )
    {

        QString window_title;
        QString class_name;
        for( ExceptionList::const_iterator iter = _exceptions.constBegin(); iter != _exceptions.constEnd(); ++iter )
        {

            // discard disabled exceptions
            if( !iter->enabled() ) continue;

            /*
            decide which value is to be compared
            to the regular expression, based on exception type
            */
            QString value;
            switch( iter->type() )
            {
                case Exception::WindowTitle:
                {
                    value = window_title.isEmpty() ? (window_title = client.caption()):window_title;
                    break;
                }

                case Exception::WindowClassName:
                {
                    if( class_name.isEmpty() )
                    {
                        // retrieve class name
                        KWindowInfo info( client.windowId(), 0, NET::WM2WindowClass );
                        QString window_class_name( info.windowClassName() );
                        QString window_class( info.windowClassClass() );
                        class_name = window_class_name + ' ' + window_class;
                    }

                    value = class_name;
                    break;
                }

                default: assert( false );

            }

            if( iter->regExp().indexIn( value ) < 0 ) continue;

            Configuration configuration( defaultConfiguration() );
            configuration.readException( *iter );
            return configuration;

        }

        return defaultConfiguration();

    }

} //namespace Oxygen
