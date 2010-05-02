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
        initialized_( false ),
        helper_( "oxygenDeco" ),
        shadowCache_( helper_ )
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

        // read in the configuration
        setInitialized( false );
        bool configuration_changed = readConfig();
        setInitialized( true );

        if( configuration_changed || (changed & (SettingDecoration | SettingButtons | SettingBorder)) )
        {

            return true;

        } else {

            if( changed & SettingColors ) shadowCache().invalidateCaches();
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

        // initialize shadow cache
        switch( defaultConfiguration().shadowCacheMode() )
        {
            case Configuration::CacheDisabled:
            {
                shadowCache_.setEnabled( false );
                break;
            }

            default:
            case Configuration::CacheVariable:
            {
                shadowCache_.setEnabled( true );
                shadowCache_.setMaxIndex( qMin( 256, int( 120*defaultConfiguration().animationsDuration()/1000 ) ) );
                break;
            }

            case Configuration::CacheMaximum:
            {
                shadowCache_.setEnabled( true );
                shadowCache_.setMaxIndex( 256 );
                break;
            }

        }

        // read exceptionsreadConfig
        ExceptionList exceptions( config );
        if( !( exceptions == exceptions_ ) )
        {
            exceptions_ = exceptions;
            changed = true;
        }

        // shadow mode
        if( configuration.shadowMode() != Configuration::OxygenShadows )
        {
            defaultConfiguration().setUseOxygenShadows( false );
            defaultConfiguration().setUseDropShadows( false );
        }

        // read shadow configurations
        ShadowConfiguration activeShadowConfiguration( QPalette::Active, config.group( "ActiveShadow" ) );
        activeShadowConfiguration.setEnabled( defaultConfiguration().useOxygenShadows() );
        if( shadowCache().shadowConfigurationChanged( activeShadowConfiguration ) )
        {
            shadowCache().setShadowConfiguration( activeShadowConfiguration );
            changed = true;
        }

        // read shadow configurations
        ShadowConfiguration inactiveShadowConfiguration( QPalette::Inactive, config.group( "InactiveShadow" ) );
        inactiveShadowConfiguration.setEnabled( defaultConfiguration().useDropShadows() );
        if( shadowCache().shadowConfigurationChanged( inactiveShadowConfiguration ) )
        {
            shadowCache().setShadowConfiguration( inactiveShadowConfiguration );
            changed = true;
        }

        if( changed )
        {

            shadowCache().invalidateCaches();
            helper().invalidateCaches();
            return true;

        } else return false;

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
            return defaultConfiguration().shadowMode() != Configuration::KWinShadows;

            case AbilityUsesAlphaChannel:
            return true;

            // tabs
            case AbilityClientGrouping:
            return defaultConfiguration().tabsEnabled();

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
        for( ExceptionList::const_iterator iter = exceptions_.constBegin(); iter != exceptions_.constEnd(); ++iter )
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

            // propagate all features found in mask to the output configuration
            if( iter->mask() & Exception::FrameBorder ) configuration.setFrameBorder( iter->frameBorder() );
            if( iter->mask() & Exception::BlendColor ) configuration.setBlendColor( iter->blendColor() );
            if( iter->mask() & Exception::DrawSeparator ) configuration.setDrawSeparator( iter->drawSeparator() );
            if( iter->mask() & Exception::TitleOutline ) configuration.setDrawTitleOutline( iter->drawTitleOutline() );
            if( iter->mask() & Exception::SizeGripMode ) configuration.setSizeGripMode( iter->sizeGripMode() );
            configuration.setHideTitleBar( iter->hideTitleBar() );
            return configuration;

        }

        return defaultConfiguration();

    }

} //namespace Oxygen
