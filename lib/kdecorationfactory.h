/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#ifndef KDECORATIONFACTORY_H
#define KDECORATIONFACTORY_H

#include <kdecoration.h>

class KDecoration;
class KDecorationBridge;
class KDecorationFactoryPrivate;

class KDecorationFactory
    : public KDecorationDefines
    {
    public:
        enum Ability { NOTHING_YET }; // FRAME pridat, + pamatovat na 32bitu?
	/**
	 * Constructor. Called after loading the decoration plugin. All global
	 * initialization of the plugin should be done in the factory constructor.
	 */
        KDecorationFactory();
	/**
	 * Destructor. Called before unloading the decoration plugin. All global
	 * cleanup of the plugin should be done in the factory destructor.
	 */
        virtual ~KDecorationFactory();
	/**
	 * This function must be reimplemented to create decoration objects.
	 * The argument should be passed to the KDecoration constructor, the second
	 * KDecoration argument should be this factory object.
	 */
        virtual KDecoration* createDecoration( KDecorationBridge* bridge ) = 0;
	/**
	 * This function is called when the configuration settings changed.
	 * The argument specifies what has changed, using the SettingXXX masks.
	 * It should be determined whether the decorations need to be completely
	 * remade, in which case true should be returned, or whether only e.g.
	 * a repaint will be sufficient, in which case false should be returned,
	 * and resetDecorations() can be called to reset all decoration objects.
	 * Note that true should be returned only when really necessary.
	 */
        virtual bool reset( unsigned long changed ); // returns true if the decoration needs to be recreated
        virtual bool supports( Ability ability );
	/**
	 * Returns the KDecorationOptions object, which is used to access
	 * configuration settings for the decoration.
	 */
        const KDecorationOptions* options(); // convenience
        /**
         * @internal
         */
        void addDecoration( KDecoration* );
        /**
         * @internal
         */
        void removeDecoration( KDecoration* );
    protected:
	/**
	 * Convenience function that calls KDecoration::reset() for all decoration
	 * objects.
	 */
        void resetDecorations( unsigned long changed ); // convenience
    private:
        QValueList< KDecoration* > _decorations;
        KDecorationFactoryPrivate* d;
    };
    
inline const KDecorationOptions* KDecorationFactory::options()
    {
    return KDecoration::options();
    }
    
#endif
