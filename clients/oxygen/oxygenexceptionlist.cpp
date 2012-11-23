//////////////////////////////////////////////////////////////////////////////
// oxygenexceptionlist.cpp
// window decoration exceptions
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
#include "oxygenutil.h"

#include <QtCore/QTextStream>

namespace Oxygen
{

    //______________________________________________________________
    void ExceptionList::readConfig( KConfig& config )
    {

        _exceptions.clear();

        QString groupName;
        for( int index = 0; KConfigGroup( &config, groupName = exceptionGroupName( index ) ).exists(); ++index )
        {

            // create exception
            Configuration exception;

            // reset group
            Util::readConfig( &exception, &config, groupName );

            // create new configuration
            ConfigurationPtr configuration( new Configuration() );
            Util::readConfig( configuration.data(), &config );

            // apply changes from exception
            configuration->setEnabled( exception.enabled() );
            configuration->setExceptionType( exception.exceptionType() );
            configuration->setExceptionPattern( exception.exceptionPattern() );
            configuration->setMask( exception.mask() );

            // propagate all features found in mask to the output configuration
            if( exception.mask() & FrameBorder ) configuration->setFrameBorder( exception.frameBorder() );
            if( exception.mask() & BlendColor ) configuration->setBlendStyle( exception.blendStyle() );
            if( exception.mask() & DrawSeparator ) configuration->setSeparatorMode( exception.separatorMode() );
            if( exception.mask() & TitleOutline ) configuration->setDrawTitleOutline( exception.drawTitleOutline() );
            if( exception.mask() & SizeGripMode ) configuration->setDrawSizeGrip( exception.drawSizeGrip() );
            configuration->setHideTitleBar( exception.hideTitleBar() );

            // append to exceptions
            _exceptions.append( configuration );

        }

    }

    //______________________________________________________________
    void ExceptionList::writeConfig( KConfig& config )
    {

        // remove all existing exceptions
        int index(0);
        while( true )
        {
            KConfigGroup group( &config, exceptionGroupName( index ) );
            if( group.exists() )
            {

                group.deleteGroup();
                ++index;

            } else break;
        }

        // rewrite current exceptions
        index = 0;
        foreach( const ConfigurationPtr& exception, _exceptions )
        {

            Util::writeConfig( exception.data(), &config, exceptionGroupName( index ) );
            ++index;

        }

    }

    //_______________________________________________________________________
    QString ExceptionList::exceptionGroupName( int index )
    {
      QString out;
      QTextStream( &out ) << "Windeco Exception " << index;
      return out;
    }

}
