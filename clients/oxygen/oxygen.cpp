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

#include "oxygen.h"
#include "oxygen.moc"
#include "oxygenclient.h"
#include "oxygenshadowcache.h"
#include "lib/helper.h"

#include <cassert>
#include <KConfigGroup>
#include <KDebug>
#include <KGlobal>
#include <KWindowInfo>
#include <kdeversion.h>

extern "C"
{
  KDE_EXPORT KDecorationFactory* create_factory()
  { return new Oxygen::OxygenFactory(); }
}

namespace Oxygen
{

  K_GLOBAL_STATIC_WITH_ARGS(OxygenHelper, globalHelper, ("oxygenDeco"))
  K_GLOBAL_STATIC_WITH_ARGS(OxygenShadowCache, globalShadowCache, (maxAnimationIndex))

  //___________________________________________
  OxygenHelper *oxygenHelper()
  {
    OxygenHelper *helper = globalHelper;
    return helper;
  }

  //___________________________________________
  OxygenShadowCache *oxygenShadowCache()
  {
    OxygenShadowCache* cache = globalShadowCache;
    return cache;
  }

  // initialize static members
  bool OxygenFactory::initialized_ = false;
  OxygenConfiguration OxygenFactory::defaultConfiguration_;
  OxygenExceptionList OxygenFactory::exceptions_;

  //___________________________________________________
  OxygenFactory::OxygenFactory()
  {
    readConfig();
    setInitialized( true );
  }

  //___________________________________________________
  OxygenFactory::~OxygenFactory()
  { setInitialized( false ); }

  //___________________________________________________
  KDecoration* OxygenFactory::createDecoration(KDecorationBridge* bridge )
  {
    OxygenClient* client( new OxygenClient( bridge, this ) );
    connect( this, SIGNAL( configurationChanged() ), client, SLOT( resetConfiguration() ) );
    return client->decoration();
  }

  //___________________________________________________
  bool OxygenFactory::reset(unsigned long changed)
  {

    kDebug( 1212 ) << endl;

    // read in the configuration
    setInitialized( false );
    bool configuration_changed = readConfig();
    setInitialized( true );

    emit configurationChanged();
    if( configuration_changed || (changed & (SettingDecoration | SettingButtons | SettingBorder)) )
    {

      return true;

    } else {

      if( changed & SettingColors ) oxygenShadowCache()->invalidateCaches();
      resetDecorations(changed);
      return false;

    }

  }

  //___________________________________________________
  bool OxygenFactory::readConfig()
  {

    kDebug( 1212 ) << endl;

    bool changed( false );

    // create a config object
    KConfig config("oxygenrc");
    KConfigGroup group( config.group("Windeco") );
    OxygenConfiguration configuration( group );
    if( !( configuration == defaultConfiguration() ) )
    {
      setDefaultConfiguration( configuration );
      changed = true;
    }

    // read exceptionsreadConfig
    OxygenExceptionList exceptions( config );
    if( !( exceptions == exceptions_ ) )
    {
      exceptions_ = exceptions;
      changed = true;
    }

    // read shadow configurations
    OxygenShadowConfiguration activeShadowConfiguration( QPalette::Active, config.group( "ActiveShadow" ) );
    if( oxygenShadowCache()->shadowConfigurationChanged( activeShadowConfiguration ) )
    {
      oxygenShadowCache()->setShadowConfiguration( activeShadowConfiguration );
      changed = true;
    }

    // read shadow configurations
    OxygenShadowConfiguration inactiveShadowConfiguration( QPalette::Inactive, config.group( "InactiveShadow" ) );
    if( oxygenShadowCache()->shadowConfigurationChanged( inactiveShadowConfiguration ) )
    {
      oxygenShadowCache()->setShadowConfiguration( inactiveShadowConfiguration );
      changed = true;
    }

    if( changed )
    {

      oxygenShadowCache()->invalidateCaches();
      oxygenHelper()->invalidateCaches();
      return true;

    } else return false;

  }

  //_________________________________________________________________
  bool OxygenFactory::supports( Ability ability ) const
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
      case AbilityUsesAlphaChannel:
      return true;

      // no colors supported at this time
      default:
      return false;
    };
  }



  //____________________________________________________________________
  OxygenConfiguration OxygenFactory::configuration( const OxygenClient& client )
  {

    QString window_title;
    QString class_name;
    for( OxygenExceptionList::const_iterator iter = exceptions_.constBegin(); iter != exceptions_.constEnd(); ++iter )
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
        case OxygenException::WindowTitle:
        {
          value = window_title.isEmpty() ? (window_title = client.caption()):window_title;
          break;
        }

        case OxygenException::WindowClassName:
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


      OxygenConfiguration configuration( defaultConfiguration() );

      // propagate all features found in mask to the output configuration
      if( iter->mask() & OxygenException::FrameBorder ) configuration.setFrameBorder( iter->frameBorder() );
      if( iter->mask() & OxygenException::BlendColor ) configuration.setBlendColor( iter->blendColor() );
      if( iter->mask() & OxygenException::DrawSeparator ) configuration.setDrawSeparator( iter->drawSeparator() );
      if( iter->mask() & OxygenException::TitleOutline ) configuration.setDrawTitleOutline( iter->drawTitleOutline() );
      if( iter->mask() & OxygenException::SizeGripMode ) configuration.setSizeGripMode( iter->sizeGripMode() );

      return configuration;

    }

    return defaultConfiguration();

  }

} //namespace Oxygen
