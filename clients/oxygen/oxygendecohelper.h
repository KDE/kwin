#ifndef oxygendecohelper_h
#define oxygendecohelper_h

/*
 * Copyright 2008 Long Huynh Huu <long.upcase@googlemail.com>
 * Copyright 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
 * Copyright 2007 Casper Boemann <cbr@boemann.dk>
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

#include "oxygenhelper.h"

//! helper class
/*! contains utility functions used at multiple places in oxygen style */
namespace Oxygen
{

    class DecoHelper : public Helper
    {

        public:

        //! constructor
        DecoHelper(const QByteArray &componentName);

        //! destructor
        virtual ~DecoHelper()
        {}

        //! dynamically allocated debug area
        int debugArea( void ) const
        { return _debugArea; }

        //! reset all caches
        virtual void invalidateCaches();

        //!@name decoration specific helper functions
        //!
        //@{
        virtual QPixmap windecoButton(const QColor &color, const QColor& glow, bool sunken, int size = 21);
        //@}

        //
        virtual QRegion decoRoundedMask( const QRect&, int left = 1, int right = 1, int top = 1, int bottom = 1 ) const;

        //! title bar text color
        const QColor& inactiveTitleBarTextColor( const QPalette& );

        //! button text color
        const QColor& inactiveButtonTextColor( const QPalette& );

        protected:

        //! reduce contrast between two colors
        QColor reduceContrast(const QColor&, const QColor&, double) const;

        private:

        //! dynamically allocated debug area
        int _debugArea;

        //! windeco buttons
        Cache<QPixmap> _windecoButtonCache;

        //! titleBar text color cache
        ColorCache _titleBarTextColorCache;

        //! button text color cache
        ColorCache _buttonTextColorCache;

    };

}

#endif // __OXYGEN_STYLE_HELPER_H
