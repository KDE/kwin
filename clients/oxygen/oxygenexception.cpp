//////////////////////////////////////////////////////////////////////////////
// oxygenexception.cpp
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
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

#include "oxygenexception.h"
#include <cassert>
#include <KLocale>

namespace Oxygen
{

  //_______________________________________________________
  Exception::Exception( KConfigGroup group ):
    Configuration( group )
  {

    // used to set default values when entries are not found in kconfig
    Exception default_configuration;

    // exception type
    setType( type(
      group.readEntry( OxygenConfig::TYPE,
      default_configuration.typeName( false ) ), false ) );

    // exception pattern
    regExp().setPattern( group.readEntry( OxygenConfig::PATTERN, QString() ) );

    // enability
    setEnabled(
      group.readEntry( OxygenConfig::ENABLED,
      default_configuration.enabled() ) );

    // exception mask
    setMask(
      group.readEntry( OxygenConfig::MASK,
      default_configuration.mask() ) );

  }

  //_______________________________________________________
  void Exception::write( KConfigGroup& group ) const
  {

    Configuration::write( group );
    group.writeEntry( OxygenConfig::TYPE, typeName( false ) );
    group.writeEntry( OxygenConfig::PATTERN, regExp().pattern() );
    group.writeEntry( OxygenConfig::ENABLED, enabled() );
    group.writeEntry( OxygenConfig::MASK, mask() );

  }

  //_______________________________________________________
  QString Exception::typeName( Type type, bool translated )
  {
    switch( type )
    {
      case WindowTitle: return translated ? i18n( "Window Title" ):"Window Title";
      case WindowClassName: return translated ? i18n( "Window Class Name" ):"Window Class Name";
      default: assert( false );
    }

    return QString();
  }

  //_______________________________________________________
  Exception::Type Exception::type( const QString& value, bool translated )
  {
    if( value == typeName( WindowTitle, translated ) ) return WindowTitle;
    else if( value == typeName( WindowClassName, translated ) ) return WindowClassName;
    else return WindowClassName;
  }

}
