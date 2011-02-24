#ifndef oxygenexception_h
#define oxygenexception_h

//////////////////////////////////////////////////////////////////////////////
// oxygenexception.h
// window decoration exception
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

#include <QtCore/QRegExp>
#include "oxygenconfiguration.h"


namespace OxygenConfig
{

    //! needed for exceptions
    static const QString TYPE = "Type";
    static const QString PATTERN = "Pattern";
    static const QString ENABLED = "Enabled";
    static const QString MASK = "Mask";

}

namespace Oxygen
{

    //! oxygen exception
    class Exception: public Configuration
    {

        public:

        //! exception type
        enum Type {
            WindowTitle,
            WindowClassName
        };

        //! mask
        enum AttributesMask {
            None = 0,
            TitleAlignment = 1<<0,
            DrawSeparator = 1<<2,
            TitleOutline = 1<<3,
            FrameBorder = 1<<4,
            BlendColor = 1<<5,
            SizeGripMode = 1<<6,
            All = TitleAlignment|DrawSeparator|TitleOutline|FrameBorder|BlendColor|SizeGripMode
        };

        //! constructor
        Exception( Configuration configuration = Configuration() ):
            Configuration( configuration ),
            _enabled( true ),
            _type( WindowClassName ),
            _mask( None )
        {}

        //! constructor
        Exception( KConfigGroup );

        //! destructor
        virtual ~Exception( void )
        {}

        //! equal to operator
        bool operator == (const Exception& exception ) const
        {
            return
                enabled() == exception.enabled() &&
                type() == exception.type() &&
                regExp().pattern() == exception.regExp().pattern() &&
                mask() == exception.mask() &&
                Configuration::operator == ( exception );
        }

        //! less than operator
        bool operator < (const Exception& exception ) const
        {
            if( enabled() != exception.enabled() ) return enabled() < exception.enabled();
            else if( mask() != exception.mask() ) return mask() < exception.mask();
            else if( type() != exception.type() ) return type() < exception.type();
            else return regExp().pattern() < exception.regExp().pattern();
        }

        //! write to kconfig group
        virtual void write( KConfigGroup& ) const;

        //!@name enability
        //@{

        bool enabled( void ) const
        { return _enabled; }

        void setEnabled( bool enabled )
        { _enabled = enabled; }

        //@}

        //!@name exception type
        //@{

        static QString typeName( Type, bool );
        static Type type( const QString& name, bool );

        virtual QString typeName( bool translated ) const
        { return typeName( type(), translated ); }

        virtual Type type( void ) const
        { return _type; }

        virtual void setType( Type value )
        { _type = value; }

        //@}

        //!@name regular expression
        //@{

        virtual QRegExp regExp( void ) const
        { return _regExp; }

        virtual QRegExp& regExp( void )
        { return _regExp; }

        //@}


        //! mask
        //!@{

        unsigned int mask( void ) const
        { return _mask; }

        void setMask( unsigned int mask )
        { _mask = mask; }

        //@}

        private:

        //! enability
        bool _enabled;

        //! exception type
        Type _type;

        //! regular expression to match window caption
        QRegExp _regExp;

        //! attributes mask
        unsigned int _mask;

    };



}

#endif
