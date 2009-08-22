// $Id: nitrogenconvertexceptions.cpp,v 1.1 2009/06/24 01:55:51 hpereira Exp $

/******************************************************************************
 *                        
 * Copyright (C) 2002 Hugo PEREIRA <mailto: hugo.pereira@free.fr>            
 *                        
 * This is free software; you can redistribute it and/or modify it under the     
 * terms of the GNU General Public License as published by the Free Software     
 * Foundation; either version 2 of the License, or (at your option) any later   
 * version.                            
 *                         
 * This software is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY;  without even the implied warranty of MERCHANTABILITY or         
 * FITNESS FOR A PARTICULAR PURPOSE.   See the GNU General Public License         
 * for more details.                    
 *                         
 * You should have received a copy of the GNU General Public License along with 
 * software; if not, write to the Free Software Foundation, Inc., 59 Temple     
 * Place, Suite 330, Boston, MA   02111-1307 USA                          
 *                        
 *                        
 *******************************************************************************/

#include <kstandarddirs.h>

#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QString>
#include <QTextStream>

#include "nitrogenexception.h"
#include "nitrogenexceptionlist.h"

using namespace std;
using namespace Nitrogen;

namespace NitrogenXml
{
  static const QString EXCEPTION_LIST = "exception_list";
  static const QString EXCEPTION = "exception";
  static const QString ENABLED = "enabled";
  static const QString TYPE = "type";
  static const QString PATTERN = "pattern";
  static const QString MASK = "mask";
};

//__________________________________________________________
NitrogenException readException( QDomElement element )
{
  
  NitrogenException out;
  
  // parse attributes
  QDomNamedNodeMap attributes( element.attributes() );
  for( unsigned int i=0; i<attributes.length(); i++ )
  {
    QDomAttr attribute( attributes.item( i ).toAttr() );
    if( attribute.isNull() ) continue;
    else if( attribute.name() == NitrogenXml::ENABLED ) out.setEnabled( attribute.value().toUInt() );
    else if( attribute.name() == NitrogenXml::TYPE ) out.setType( NitrogenException::type( attribute.value() ) );
    else if( attribute.name() == NitrogenXml::PATTERN ) out.regExp().setPattern( attribute.value() );
    else if( attribute.name() == NitrogenConfig::FRAME_BORDER ) out.setFrameBorder( attribute.value() );
    else if( attribute.name() == NitrogenConfig::BLEND_COLOR ) out.setBlendColor( attribute.value() );
    else if( attribute.name() == NitrogenConfig::DRAW_SEPARATOR ) out.setDrawSeparator( attribute.value().toInt() );
    else if( attribute.name() == NitrogenConfig::SHOW_STRIPES ) out.setShowStripes( attribute.value().toInt() );
    else if( attribute.name() == NitrogenConfig::DRAW_SIZE_GRIP ) out.setDrawSizeGrip( attribute.value().toInt() );
    else if( attribute.name() == NitrogenXml::MASK ) out.setMask( attribute.value().toUInt() );
    
    // this is kept for backward compatibility
    else if( attribute.name() == "blend_color" ) out.setBlendColor( attribute.value() );
    
  }
 
  return out;
  
}

//__________________________________________________________
NitrogenExceptionList readExceptions( QDomDocument document )
{
  NitrogenExceptionList out;
  
  // parse
  QDomElement doc_element = document.documentElement();
  for(QDomNode node = doc_element.firstChild(); !node.isNull(); node = node.nextSibling() )
  {
    
    QDomElement element = node.toElement();
    if( element.isNull() ) continue;
    
    if( element.tagName() == NitrogenXml::EXCEPTION ) 
    { out.push_back( readException( element ) ); }
    
  }

  return out;
  
}

//__________________________________________________________
int main( int argc, char *argv[] )
{
  // the core application is needed to have locale, fonts, etc. set properly, notably for QSting
  // not having it might result in lost accents and special characters.
  QCoreApplication application( argc, argv );
  
  // generate filename
  QString filename = KStandardDirs::locateLocal( "config", "nitrogen_xml_rc" );
  QTextStream( stdout ) << "nitrogen-convert-exceptions - reading file " << filename << endl;
  
  // open and get DomDocument
  QFile file( filename );
  if ( !file.open( QIODevice::ReadOnly ) )
  {
    QTextStream( stdout ) << "nitrogen-convert-exceptions - invalid file" << endl;
    return 0;
  }
  
  // read dom document
  QDomDocument document;
  if ( !document.setContent( &file ) ) 
  {
    file.close();
    QTextStream( stdout ) << "nitrogen-convert-exceptions - invalid file" << endl;
    return 0;
  }
  
  NitrogenExceptionList exceptions( readExceptions( document ) );
  QTextStream( stdout ) << "nitrogen-convert-exceptions - read " << exceptions.size() << " exceptions" << endl;
  
  // write to kconfig
  KConfig configuration( "nitrogenrc" );
  exceptions.write( configuration );
  configuration.sync();
  
  return 0;
  
}
