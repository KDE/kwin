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
#include <kdecorations_export.h>

/** @addtogroup kdecoration */
/** @{ */

class KDecoration;
class KDecorationBridge;
class KDecorationFactoryPrivate;

class KDECORATIONS_EXPORT KDecorationFactory
    : public QObject, public KDecorationDefines
{
    Q_OBJECT
public:
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
    virtual KDecoration* createDecoration(KDecorationBridge* bridge) = 0;

    /**
     * Reimplement this function if your decoration supports more border sizes than
     * the default one (BorderNormal). The returned list must contain all supported
     * sizes, ordered from the smallest to the largest one. By default, only
     * BorderNormal is returned.
     */
    virtual QList< BorderSize > borderSizes() const;

    virtual bool supports(Ability ability) const = 0;

    virtual void checkRequirements(KDecorationProvides* provides);
    /**
     * Returns true if the given decoration object still exists. This is necessary
     * e.g. when calling KDecoration::showWindowMenu(), which may cause the decoration
     * to be destroyed. Note that this function is reliable only if called immediately
     * after such actions.
     */
    bool exists(const KDecoration* deco) const;

    /**
     * Set & get the position of the close button - most decorations don't have to call this ever.
     *
     * By default, the legacy position indicated by the options (top left or top right) will be
     * returned.
     * Only if you need to provide a bottom corner or your decoration does not respect those
     * settings you will have to specify the exact corner (eg. used by the "present windows"
     * closer)
     *
     * @since 4.8
     */
    Qt::Corner closeButtonCorner();
    void setCloseButtonCorner(Qt::Corner cnr);

    /**
     * @internal
     */
    void addDecoration(KDecoration*);
    /**
     * @internal
     */
    void removeDecoration(KDecoration*);

Q_SIGNALS:
    /**
     * @brief An implementing class should emit this signal if it's decorations should
     * be recreated. For example after a setting changed in a way that the only logical
     * step is to recreate the decoration.
     */
    void recreateDecorations();
protected:
    /**
     * Constructor. Called after loading the decoration plugin. All global
     * initialization of the plugin should be done in the factory constructor.
     */
    explicit KDecorationFactory(QObject *parent = nullptr);
    /**
     * This function has the same functionality like KDecoration::windowType().
     * It can be used in createDecoration() to return different KDecoration
     * inherited classes depending on the window type, as at that time
     * KDecoration::windowType() is not available yet. The additional argument
     * is the one passed to createDecoration().
     */
    NET::WindowType windowType(unsigned long supported_types, KDecorationBridge* bridge) const;
    /**
     * Returns the KDecorationOptions object, which is used to access
     * configuration settings for the decoration.
     *
     * @deprecated use KDecorationOptions::self()
     */
    const KDecorationOptions* options(); // convenience
private:
    KDecorationFactoryPrivate* d;
};

/** @} */

#endif
