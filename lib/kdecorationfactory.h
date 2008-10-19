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

#include "kdecoration.h"

/** @addtogroup kdecoration */
/** @{ */

class KDecoration;
class KDecorationBridge;
class KDecorationFactoryPrivate;

class KWIN_EXPORT KDecorationFactory
    : public KDecorationDefines
    {
    public:
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

        /**
         * Reimplement this function if your decoration supports more border sizes than
         * the default one (BorderNormal). The returned list must contain all supported
         * sizes, ordered from the smallest to the largest one. By default, only
         * BorderNormal is returned.
         */        
        virtual QList< BorderSize > borderSizes() const;

        virtual bool supports( Ability ability ) const = 0;
        
        virtual void checkRequirements( KDecorationProvides* provides );
        /**
         * Returns the KDecorationOptions object, which is used to access
         * configuration settings for the decoration.
         */
        const KDecorationOptions* options(); // convenience
        /**
         * Returns true if the given decoration object still exists. This is necessary
         * e.g. when calling KDecoration::showWindowMenu(), which may cause the decoration
         * to be destroyed. Note that this function is reliable only if called immediately
         * after such actions.
         */
        bool exists( const KDecoration* deco ) const;
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
        /**
         * This function has the same functionality like KDecoration::windowType().
         * It can be used in createDecoration() to return different KDecoration
         * inherited classes depending on the window type, as at that time
         * KDecoration::windowType() is not available yet. The additional argument
         * is the one passed to createDecoration().
         */
        NET::WindowType windowType( unsigned long supported_types, KDecorationBridge* bridge ) const;
    private:
        QList< KDecoration* > _decorations;
        KDecorationFactoryPrivate* d;
    };

/**
 * @warning THIS CLASS IS UNSTABLE AND WILL ONLY BE SUPPORTED IN KDE 4.2!
 */
class KWIN_EXPORT KDecorationFactoryUnstable
    : public KDecorationFactory
    {
    public:
        /**
         * This function should return the texture lists that contain the textures of the
         * shadow quads. Textures are mapped to the quad that has the same list offset.
         * E.g. texture[0][2] is rendered where the third QRect that shadowQuads()
         * returns is if using the first texture list.
         */
        virtual QList< QList<QImage> > shadowTextures();
        /**
         * This function should return the texture list offset for the requested type.
         */
        virtual int shadowTextureList( ShadowType type ) const;
        /**
         * This function should return the positions of the shadow quads to be rendered.
         * All positions are relative to the window's top-left corner. Only "borderless"
         * and "other" types will call this method.
         * @param   size    The size of the window.
         */
        virtual QList<QRect> shadowQuads( ShadowType type, QSize size ) const;
        /**
         * This function should return the desired opacity of the shadow.
         */
        virtual double shadowOpacity( ShadowType type ) const;
        /**
         * This function should return the desired brightness of the shadow.
         */
        virtual double shadowBrightness( ShadowType type ) const;
        /**
         * This function should return the desired saturation of the shadow.
         */
        virtual double shadowSaturation( ShadowType type ) const;
    };

inline const KDecorationOptions* KDecorationFactory::options()
    {
    return KDecoration::options();
    }

/** @} */
    
#endif
