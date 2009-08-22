//////////////////////////////////////////////////////////////////////////////
// Nitrogenbutton.h
// -------------------
// Nitrogen window decoration for KDE.
// -------------------
// Copyright (c) 2006, 2007 Riccardo Iaconelli <riccardo@kde.org>
// Copyright (c) 2009, 2010 Hugo Pereira <hugo.pereira@free.fr>
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

#include <assert.h>
#include <kconfiggroup.h>
#include <kdeversion.h>
#include <kwindowinfo.h>
#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QTextStream>

#include "nitrogen.h"
#include "nitrogen.moc"
#include "nitrogenclient.h"


extern "C"
{
  KDE_EXPORT KDecorationFactory* create_factory()
  { return new Nitrogen::NitrogenFactory(); }
}

namespace Nitrogen
{
  
  // referenced from definition in Nitrogendclient.cpp
  NitrogenHelper *nitrogenHelper(); 
  
  // initialize static members
  bool NitrogenFactory::initialized_ = false;
  NitrogenConfiguration NitrogenFactory::defaultConfiguration_;
  NitrogenExceptionList NitrogenFactory::exceptions_;
  
  //___________________________________________________
  NitrogenFactory::NitrogenFactory()
  {
    readConfig();
    setInitialized( true );
  }
  
  //___________________________________________________
  NitrogenFactory::~NitrogenFactory() 
  { setInitialized( false ); }
  
  //___________________________________________________
  KDecoration* NitrogenFactory::createDecoration(KDecorationBridge* bridge )
  { 
    NitrogenClient* client( new NitrogenClient( bridge, this ) );
    connect( this, SIGNAL( configurationChanged() ), client, SLOT( resetConfiguration() ) );
    return client->decoration(); 
  }
  
  //___________________________________________________
  bool NitrogenFactory::reset(unsigned long changed)
  {
    
    QTextStream( stdout ) << "NitrogenConfig::reset." << endl;
    
    // read in the configuration
    setInitialized( false );
    bool configuration_changed = readConfig();
    setInitialized( true );
    
    if( configuration_changed || (changed & (SettingDecoration | SettingButtons | SettingBorder)) ) 
    { 
      
      emit configurationChanged();
      return true; 
      
    } else {
      
      emit configurationChanged();
      resetDecorations(changed);
      return false;
      
    }
    
  }
  
  //___________________________________________________
  bool NitrogenFactory::readConfig()
  {

    QTextStream( stdout ) << "NitrogenConfig::readConfig." << endl;
    
    // create a config object
    KConfig config("nitrogenrc");
    KConfigGroup group( config.group("Windeco") );
    NitrogenConfiguration configuration( group );
    bool changed = !( configuration == defaultConfiguration() );

    // check configurations
    changed |= (NitrogenConfiguration::useCompiz() == NitrogenConfiguration::checkUseCompiz());
    
    // read exceptionsreadConfig
    NitrogenExceptionList exceptions( config );
    if( !( exceptions == exceptions_ ) ) 
    {
      exceptions_ = exceptions; 
      changed = true;
    }
    
    
    if( changed ) 
    {
      
      nitrogenHelper()->invalidateCaches();
      setDefaultConfiguration( configuration );
      return true;
      
    } else return false;
    
  }
  
  //_________________________________________________________________
  bool NitrogenFactory::supports( Ability ability ) const
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
      #if KDE_IS_VERSION(4,2,92)
      case AbilityProvidesShadow: // TODO: UI option to use default shadows instead
      case AbilityUsesAlphaChannel:
      return !NitrogenConfiguration::useCompiz();
      
      #else
      case AbilityCompositingShadow: 
      return !NitrogenConfiguration::useCompiz();
      #endif
      
      // no colors supported at this time
      default:
      return false;
    };
  }
  
  
  #if !KDE_IS_VERSION(4,2,92)
  
  //////////////////////////////////////////////////////////////////////////////
  // Shadows (kde4.2 version)
  
  //_________________________________________________________________
  QList< QList<QImage> > NitrogenFactory::shadowTextures()
  {
    
    QList< QList<QImage> > textureLists;
    
    // in compiz mode, no textures are used
    if( NitrogenConfiguration::useCompiz() ) 
    { return textureLists; }
    
    textureLists.append( makeTextures( defaultConfiguration().useOxygenShadows() ? Active:Inactive ) );
    textureLists.append( makeTextures( Inactive ) );
    return textureLists;

  }
    
  //_________________________________________________________________
  QList<QImage> NitrogenFactory::makeTextures( NitrogenFactory::Activity state ) const
  {
    
    #define shadowFuzzyness 10
    #define shadowSize 10

    // palette
    QPalette palette = qApp->palette();                                                
    palette.setCurrentColorGroup(QPalette::Active);                                    
    
    // shadow size
    qreal size = 25.5;

    // prepare pixmap and painter
    QPixmap shadow( size*2, size*2 );
    shadow.fill( Qt::transparent );
    QPainter p( &shadow );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    
    // prepare colors
    QColor color = palette.window().color();
    QColor light = nitrogenHelper()->calcLightColor(nitrogenHelper()->backgroundTopColor(color));
    QColor dark = nitrogenHelper()->calcDarkColor(nitrogenHelper()->backgroundBottomColor(color));
    
    QLinearGradient lg = QLinearGradient(0.0, size-4.5, 0.0, size+4.5);
    if( defaultConfiguration().frameBorder() <= NitrogenConfiguration::BorderTiny )
    {
      
      lg.setColorAt(0, nitrogenHelper()->backgroundTopColor(color) );
      lg.setColorAt(0.52, nitrogenHelper()->backgroundTopColor(color));
      lg.setColorAt(1.0, nitrogenHelper()->backgroundBottomColor(color) );
      
    } else {
      
      lg.setColorAt(0.52, light);
      lg.setColorAt(1.0, dark);
    
    }

    switch( state )
    {
      case Active:
      {
        

        QColor glow = KDecoration::options()->color(ColorFrame);
        QColor glow2 = KDecoration::options()->color(ColorTitleBar);
        
        QRadialGradient rg( size, size, size );
        QColor c = color;
        c.setAlpha( 255 );  rg.setColorAt( 4.4/size, c );
        c = glow;
        c.setAlpha( 220 );  rg.setColorAt( 4.5/size, c );
        c.setAlpha( 180 );  rg.setColorAt( 5/size, c );
        c.setAlpha( 25 );  rg.setColorAt( 5.5/size, c );
        c.setAlpha( 0 );  rg.setColorAt( 6.5/size, c );
        p.setBrush( rg );
        p.drawRect( shadow.rect() );
        
        rg = QRadialGradient( size, size, size );
        c = color;
        c.setAlpha( 255 );  rg.setColorAt( 4.4/size, c );
        c = glow2;
        c.setAlpha( 0.58*255 );  rg.setColorAt( 4.5/size, c );
        c.setAlpha( 0.43*255 );  rg.setColorAt( 5.5/size, c );
        c.setAlpha( 0.30*255 );  rg.setColorAt( 6.5/size, c );
        c.setAlpha( 0.22*255 );  rg.setColorAt( 7.5/size, c );
        c.setAlpha( 0.15*255 );  rg.setColorAt( 8.5/size, c );
        c.setAlpha( 0.08*255 );  rg.setColorAt( 11.5/size, c );
        c.setAlpha( 0);  rg.setColorAt( 14.5/size, c );
        p.setBrush( rg );
        p.drawRect( shadow.rect() );
        
        // draw the corner of the window - actually all 4 corners as one circle
        p.setBrush( Qt::NoBrush );
        p.setPen(QPen(lg, 0.8));
        p.drawEllipse(QRectF(size-4, size-4, 8, 8));
        
        // cut out the part of the texture that is under the window
        p.setCompositionMode( QPainter::CompositionMode_DestinationOut );
        p.setBrush( QColor( 0, 0, 0, 255 ));
        p.setPen( Qt::NoPen );
        p.drawEllipse(QRectF(size-3, size-3, 6, 6));
        p.drawRect(QRectF(size-3, size-1, 6, 2));
        p.drawRect(QRectF(size-1, size-3, 2, 6));
        p.end();
        
        return makeTextures( shadow );
        
      }
      
      case Inactive: 
      {
        
        //---------------------------------------------------------------
        // Inactive shadow texture
        
        QRadialGradient rg( size, size+4, size );
        QColor c( Qt::black );
        c.setAlpha( 0.12*255 );  rg.setColorAt( 4.5/size, c );
        c.setAlpha( 0.11*255 );  rg.setColorAt( 6.6/size, c );
        c.setAlpha( 0.075*255 );  rg.setColorAt( 8.5/size, c );
        c.setAlpha( 0.06*255 );  rg.setColorAt( 11.5/size, c );
        c.setAlpha( 0.035*255 );  rg.setColorAt( 14.5/size, c );
        c.setAlpha( 0.025*255 );  rg.setColorAt( 17.5/size, c );
        c.setAlpha( 0.01*255 );  rg.setColorAt( 21.5/size, c );
        c.setAlpha( 0.0*255 );  rg.setColorAt( 25.5/size, c );
        p.setBrush( rg );
        p.drawRect( shadow.rect() );
        
        rg = QRadialGradient( size, size+2, size );
        c = QColor( Qt::black );
        c.setAlpha( 0.25*255 );  rg.setColorAt( 4.5/size, c );
        c.setAlpha( 0.20*255 );  rg.setColorAt( 5.5/size, c );
        c.setAlpha( 0.13*255 );  rg.setColorAt( 7.5/size, c );
        c.setAlpha( 0.06*255 );  rg.setColorAt( 8.5/size, c );
        c.setAlpha( 0.015*255 );  rg.setColorAt( 11.5/size, c );
        c.setAlpha( 0.0*255 );  rg.setColorAt( 14.5/size, c );
        p.setBrush( rg );
        p.drawRect( shadow.rect() );
        
        rg = QRadialGradient( size, size+0.2, size );
        c = QColor( Qt::black );
        c.setAlpha( 0.35*255 );  rg.setColorAt( 0/size, c );
        c.setAlpha( 0.32*255 );  rg.setColorAt( 4.5/size, c );
        c.setAlpha( 0.22*255 );  rg.setColorAt( 5.0/size, c );
        c.setAlpha( 0.03*255 );  rg.setColorAt( 5.5/size, c );
        c.setAlpha( 0.0*255 );  rg.setColorAt( 6.5/size, c );
        p.setBrush( rg );
        p.drawRect( shadow.rect() );
        
        rg = QRadialGradient( size, size, size );
        c = color;
        c.setAlpha( 255 );  rg.setColorAt( 4.0/size, c );
        c.setAlpha( 0 );  rg.setColorAt( 4.01/size, c );
        p.setBrush( rg );
        p.drawRect( shadow.rect() );
        
        // draw the corner of the window - actually all 4 corners as one circle
        p.setBrush( Qt::NoBrush );
        p.setPen(QPen(lg, 0.8));
        p.drawEllipse(QRectF(size-4, size-4, 8, 8));
        
        // cut out the part of the texture that is under the window
        p.setCompositionMode( QPainter::CompositionMode_DestinationOut );
        p.setBrush( QColor( 0, 0, 0, 255 ));
        p.setPen( Qt::NoPen );
        p.drawEllipse(QRectF(size-3, size-3, 6, 6));
        p.drawRect(QRectF(size-3, size-1, 6, 2));
        p.drawRect(QRectF(size-1, size-3, 2, 6));
        
        p.end();
        
        return makeTextures( shadow );
      }
      
      default:
      return QList<QImage>();
      
    }
  }
  
  //______________________________________________________
  QImage NitrogenFactory::makeTexture( const QPixmap& pixmap, const QRect& rect ) const
  {
    QPixmap dump( rect.size() );
    dump.fill( Qt::transparent );
    QPainter painter( &dump );
    painter.drawPixmap( QPoint(0, 0), pixmap, rect );
    painter.end();
    return dump.toImage();
  }
  
  //___________________________________________________________
  QList<QImage> NitrogenFactory::makeTextures( const QPixmap& shadow ) const
  {
    QList<QImage> textures;
    int w = shadow.width() / 2;
    int h = shadow.height() / 2;
    textures.append( makeTexture( shadow, QRect( 0, h+1, w, h ) ) );
    textures.append( makeTexture( shadow, QRect( w, h+1, 1, h ) ) );
    textures.append( makeTexture( shadow, QRect( w+1, h+1, w, h ) ) );
    textures.append( makeTexture( shadow, QRect( 0, h, w, 1 ) ) );
    textures.append( makeTexture( shadow, QRect( w+1, h, w, 1 ) ) );
    textures.append( makeTexture( shadow, QRect( 0, 0, w, h ) ) );
    textures.append( makeTexture( shadow, QRect( w, 0, 1, h ) ) );
    textures.append( makeTexture( shadow, QRect( w+1, 0, w, h ) ) );
    return textures;
  }

  //_________________________________________________________________
  int NitrogenFactory::shadowTextureList( ShadowType type ) const
  {
    switch( type ) {
      
      case ShadowBorderedInactive: return 1;
      default: return 0;
      
    }
    abort(); // Should never be reached
  }
  
  //_________________________________________________________________
  QList<QRect> NitrogenFactory::shadowQuads( ShadowType, QSize ) const
  { return QList<QRect>(); }
  
  //____________________________________________________________________
  double NitrogenFactory::shadowOpacity( ShadowType ) const
  { return 1.0; }
  #endif
  
  //____________________________________________________________________
  NitrogenConfiguration NitrogenFactory::configuration( const NitrogenClient& client )
  {
    
    QString window_title;
    QString class_name;
    for( NitrogenExceptionList::const_iterator iter = exceptions_.begin(); iter != exceptions_.end(); iter++ )
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
        case NitrogenException::WindowTitle: 
        {
          value = window_title.isEmpty() ? (window_title = client.caption()):window_title;
          break;
        }
        
        case NitrogenException::WindowClassName:
        {
          if( class_name.isEmpty() )
          {
            // retrieve class name
            KWindowInfo info( client.windowId(), 0, NET::WM2WindowClass );
            QString window_class_name( info.windowClassName() );
            QString window_class( info.windowClassClass() );
            class_name = window_class_name + " " + window_class;
          } 
          
          value = class_name;
          break;
        }
        
        default: assert( false );
        
      }
      
      if( iter->regExp().indexIn( value ) < 0 ) continue;
      
      
      NitrogenConfiguration configuration( defaultConfiguration() );
      
      // propagate all features found in mask to the output configuration
      if( iter->mask() & NitrogenException::FrameBorder ) configuration.setFrameBorder( iter->frameBorder() );
      if( iter->mask() & NitrogenException::BlendColor ) configuration.setBlendColor( iter->blendColor() );
      if( iter->mask() & NitrogenException::DrawSeparator ) configuration.setDrawSeparator( iter->drawSeparator() );
      if( iter->mask() & NitrogenException::ShowStripes ) configuration.setShowStripes( iter->showStripes() );
      if( iter->mask() & NitrogenException::OverwriteColors ) configuration.setOverwriteColors( iter->overwriteColors() );
      if( iter->mask() & NitrogenException::DrawSizeGrip ) configuration.setDrawSizeGrip( iter->drawSizeGrip() );
      
      return configuration;
      
    }
    
    return defaultConfiguration();
    
  }
  
} //namespace Nitrogen
