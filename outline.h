/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_OUTLINE_H
#define KWIN_OUTLINE_H
#include <X11/X.h>
#include <fixx11h.h>
#include <QtCore/QRect>
#include <QtCore/QVector>

namespace KWin {

/**
 * @short This class is used to show the outline of a given geometry.
 *
 * The class renders an outline by using four windows. One for each border of
 * the geometry. It is possible to replace the outline with an effect. If an
 * effect is available the effect will be used, otherwise the outline will be
 * rendered by using the X implementation.
 *
 * @author Arthur Arlt
 * @since 4.7
 */
class Outline {
public:
    Outline();
    ~Outline();

    /**
     * Set the outline geometry.
     * To show the outline use @link showOutline.
     * @param outlineGeometry The geometry of the outline to be shown
     * @see showOutline
     */
    void setGeometry(const QRect &outlineGeometry);

    /**
     * Shows the outline of a window using either an effect or the X implementation.
     * To stop the outline process use @link hideOutline.
     * @see hideOutline
     */
    void show();

    /**
     * Shows the outline for the given @p outlineGeometry.
     * This is the same as setOutlineGeometry followed by showOutline directly.
     * To stop the outline process use @link hideOutline.
     * @param outlineGeometry The geometry of the outline to be shown
     * @see hideOutline
     */
    void show(const QRect &outlineGeometry);

    /**
     * Hides shown outline.
     * @see showOutline
     */
    void hide();

    /**
     * Return outline window ids
     * @return The window ids created to represent the outline
     */
    QVector<Window> windowIds() const;
private:

    /**
     * Show the window outline using the X implementation
     */
    void showWithX();

    /**
     * Hide previously shown outline used the X implementation
     */
    void hideWithX();

    Window m_topOutline;
    Window m_rightOutline;
    Window m_bottomOutline;
    Window m_leftOutline;
    QRect m_outlineGeometry;
    bool m_initialized;
    bool m_active;
};

}

#endif
