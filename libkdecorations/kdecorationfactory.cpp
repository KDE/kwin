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

#include "kdecorationfactory.h"

#include <assert.h>

#include "kdecorationbridge.h"

class KDecorationFactoryPrivate {
public:
    KDecorationFactoryPrivate() {
        closeButtonCorner = (Qt::Corner)0;
    }
    Qt::Corner closeButtonCorner;
};

KDecorationFactory::KDecorationFactory() : d(new KDecorationFactoryPrivate)
{
}

KDecorationFactory::~KDecorationFactory()
{
    delete d;
    assert(_decorations.count() == 0);
}

bool KDecorationFactory::reset(unsigned long)
{
    return true;
}

void KDecorationFactory::checkRequirements(KDecorationProvides*)
{
}

QList< KDecorationDefines::BorderSize > KDecorationFactory::borderSizes() const
{
    return QList< BorderSize >() << BorderNormal;
}

bool KDecorationFactory::exists(const KDecoration* deco) const
{
    return _decorations.contains(const_cast< KDecoration* >(deco));
}

Qt::Corner KDecorationFactory::closeButtonCorner()
{
    if (d->closeButtonCorner)
        return d->closeButtonCorner;
    return options()->titleButtonsLeft().contains('X') ? Qt::TopLeftCorner : Qt::TopRightCorner;
}

void KDecorationFactory::setCloseButtonCorner(Qt::Corner cnr)
{
    d->closeButtonCorner = cnr;
}

void KDecorationFactory::addDecoration(KDecoration* deco)
{
    _decorations.append(deco);
}

void KDecorationFactory::removeDecoration(KDecoration* deco)
{
    _decorations.removeAll(deco);
}

void KDecorationFactory::resetDecorations(unsigned long changed)
{
    for (QList< KDecoration* >::ConstIterator it = _decorations.constBegin();
            it != _decorations.constEnd();
            ++it)
        (*it)->reset(changed);
}

NET::WindowType KDecorationFactory::windowType(unsigned long supported_types, KDecorationBridge* bridge) const
{
    return bridge->windowType(supported_types);
}
