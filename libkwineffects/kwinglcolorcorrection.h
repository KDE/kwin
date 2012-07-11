/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Casian Andrei <skeletk13@gmail.com>

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

#ifndef KWIN_COLOR_CORRECTION_H
#define KWIN_COLOR_CORRECTION_H

#include "kwinglutils_funcs.h"

#include <QObject>
#include <QMap>
#include <QRect>

namespace KWin {

class ColorCorrectionPrivate;

/**
 * Implements a color correction mechanism. The settings are obtained
 * asynchronously via D-Bus from kolor-server, which is part of kolor-manager.
 *
 * If it fails to get the settings, nothing should happen (no correction), even
 * if it is set to enabled.
 *
 * Supports per-output and per-region correction (window region).
 *
 * \warning This class is not designed to be used by effects, however
 * it may happen to be useful their case somehow.
 */
class KWIN_EXPORT ColorCorrection : public QObject
{
    Q_OBJECT

public:
    static ColorCorrection *instance();
    static void cleanup();

    /**
     * The beginning of a region list for a window \param w
     * \return Constant iterator to the beginning
     * \see setupForRegion
     *
     * It can be used to indicate a particular region for which
     * to set up color correction.
     *
     * These regions come from the applications, which communicate with the
     * color server (kolor-server in kolor-manager), and this has no direct
     * relation to KWin. Consequently, modifying these regions does not
     * make sense inside KWin.
     */
    QMap<Window, QRect>::const_iterator regionsBegin(Window w);

    /**
     * The ending of a region list for a window \param w
     * \return Constant iterator to the ending
     * \see setupForRegion
     */
    QMap<Window, QRect>::const_iterator regionsEnd(Window w);

    /**
     * Prepares color correction for the output number \param screen.
     * Sets up the appropriate color lookup texture for the output.
     */
    void setupForOutput(int screen);

    /**
     * Prepares color correction for one region of a window (in fact a rect),
     * which is indicated by \param regionIt. This can be obtained by iterating
     * between \ref regionsBegin and \ref regionsEnd, for that particular window.
     *
     * \note This should be called when drawing a window
     */
    void setupForRegion(const QMap<Window, QRect>::const_iterator &regionIt);

    /**
     * Disables color correction for a particular window region. Instead,
     * color correction is set up again for the last output that was set
     * with \ref setupForOutput.
     *
     * \note This should be called after finishing the drawing of a window.
     */
    void resetForRegion();

    /**
     * Unsets color correction by using a dummy color lookup texture. This
     * does not disable anything, the CC mechanisms remain in place. Instead, it
     * indicates to draw normally.
     */
    void reset();

    /**
     * When color correction is disabled, it does nothing and returns
     * \param sourceCode.
     *
     * Else, it modifies \param sourceCode, making it suitable for performing
     * color correction. This is done by inserting a 3d texture lookup operation
     * just before the output fragment color is returned.
     */
    QByteArray prepareFragmentShader(const QByteArray &sourceCode);

public slots:
    /**
     * Enables or disables color correction. Compositing should be restarted
     * for changes to take effect.
     */
    void setEnabled(bool enabled);

signals:
    /**
     * Emitted when some changes happened to the color correction settings, and
     * a full repaint of the scene should be done to make the new settings visible.
     */
    void changed();

private:
    ColorCorrection();
    virtual ~ColorCorrection();

private:
    ColorCorrectionPrivate *d_ptr;
    Q_DECLARE_PRIVATE(ColorCorrection)
    static ColorCorrection *s_colorCorrection;
};

} // KWin namespace

#endif // KWIN_COLOR_CORRECTION_H
