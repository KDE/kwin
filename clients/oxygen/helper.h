/*
 * Copyright 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __OXYGEN_WINDECO_HELPER_H
#define __OXYGEN_WINDECO_HELPER_H

#include "lib/helper.h"

class OxygenWindecoHelper : public OxygenHelper
{
public:
    explicit OxygenWindecoHelper(const QByteArray &componentName);
    virtual ~OxygenWindecoHelper() {}

    QPixmap windecoButton(const QColor &color, int size);

protected:
    QCache<quint64, QPixmap> m_windecoButtonCache;
};

#endif // __OXYGEN_WINDECO_HELPER_H
