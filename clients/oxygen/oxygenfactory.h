#ifndef oxygenfactory_h
#define oxygenfactory_h

//////////////////////////////////////////////////////////////////////////////
// oxygen.h
// -------------------
//
// Copyright (c) 2009 Hugo Pereira <hugo.pereira@free.fr>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
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

#include "oxygenconfiguration.h"
#include "oxygendecorationdefines.h"
#include "oxygendecohelper.h"
#include "oxygenshadowcache.h"

#include <QObject>
#include <kdecorationfactory.h>
#include <kdeversion.h>

namespace Oxygen
{

    class Client;

    enum {

        //! this is the top title bar edge
        TFRAMESIZE = 3,

        /*!
        this is the extra title bar top and bottom edges
        needed to outline active window title bar
        */
        HFRAMESIZE = 4
    };

    //! window decoration factory
    class Factory: public KDecorationFactory
    {

        Q_OBJECT

        public:

        //! constructor
        explicit Factory(QObject *parent = nullptr);

        //! destructor
        virtual ~Factory();

        //! create decoration
        virtual KDecoration *createDecoration(KDecorationBridge *b);

        //! configuration reset
        virtual bool reset(unsigned long changed);

        //! configuration capabilities
        virtual bool supports( Ability ability ) const;

        //! true if initialized
        virtual bool initialized()
        { return _initialized; }

        //! helper
        virtual DecoHelper& helper( void )
        { return _helper; }

        //! shadow cache
        virtual ShadowCache& shadowCache( void )
        { return _shadowCache; }

        //! shadow cache
        virtual const ShadowCache& shadowCache( void ) const
        { return _shadowCache; }

        //! pointer to configuration
        typedef QSharedPointer<Configuration> ConfigurationPtr;

        //! get configuration for a give client
        virtual ConfigurationPtr configuration( const Client& );

        protected:

        //! read configuration from KConfig
        void readConfig();

        //! initialization
        void setInitialized( bool value )
        { _initialized = value; }

        //! exception group name
        QString exceptionGroupName( int index ) const
        {
            QString out;
            QTextStream( &out ) << "Windeco Exception " << index;
            return out;
        }

        private:

        //! initialization flag
        bool _initialized;

        //! helper
        DecoHelper _helper;

        //! shadow cache
        ShadowCache _shadowCache;

        //! default configuration
        ConfigurationPtr _defaultConfiguration;

        //! list of exceptiosn
        QList<ConfigurationPtr> _exceptions;

    };

}

#endif
