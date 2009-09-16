//////////////////////////////////////////////////////////////////////////////
// oxygenexceptionlist.cpp
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

#include "oxygenexceptionlist.h"

#include <QTextStream>

namespace Oxygen
{

  //______________________________________________________________
  void OxygenExceptionList::read( const KConfig& config )
  {

    clear();

    for( int index = 0; true; index++ )
    {

      KConfigGroup group( &config, exceptionGroupName( index ) );
      if( group.exists() )
      {
        OxygenException exception( group );
        if( exception.regExp().isValid() ) push_back( exception );
      } else break;

    }

  }

  //______________________________________________________________
  void OxygenExceptionList::write( KConfig& config )
  {

    // remove previous group
    for( int index = 0; true ;index++ )
    {
      KConfigGroup group( &config, exceptionGroupName( index ) );
      if( group.exists() ) group.deleteGroup();
      else break;
    }

    // also add exceptions
    int index(0);
    for( OxygenExceptionList::const_iterator iter = constBegin(); iter != constEnd(); iter++, index++ )
    {

      KConfigGroup group( &config, exceptionGroupName( index ) );
      iter->write( group );

    }


  }

  //______________________________________________________________
  OxygenExceptionList OxygenExceptionList::defaultList( void )
  {

    OxygenExceptionList out;

    // default exception that covers most commonly used gtk based applications
    OxygenException exception;
    exception.setType( OxygenException::WindowClassName );
    exception.regExp().setPattern( "(Firefox)|(Thunderbird)|(Gimp)" );
    exception.setBlendColor( OxygenException::NoBlending );
    exception.setMask( OxygenException::BlendColor );
    exception.setEnabled( true );

    out.push_back( exception );
    return out;

  }

  //_______________________________________________________________________
  QString OxygenExceptionList::exceptionGroupName( int index )
  {
    QString out;
    QTextStream( &out ) << "Windeco Exception " << index;
    return out;
  }


}
