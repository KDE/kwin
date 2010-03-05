/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  if not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#ifndef BLURSHADER_H
#define BLURSHADER_H

#include <kwinglutils.h>

namespace KWin
{

class BlurShader
{
public:
    BlurShader();
    ~BlurShader();

    // Sets the radius in pixels
    void setRadius(int radius);

    // Sets the blur direction
    void setDirection(Qt::Orientation orientation);

    // Sets the distance between two pixels
    void setPixelDistance(float val);

    // The opacity of the resulting pixels is multiplied by this value
    void setOpacity(float val);

    void bind();
    void unbind();

private:
    float gaussian(float x, float sigma) const;
    QVector<float> gaussianKernel() const;
    void init();

private:
    GLuint program;
    int radius;
    Qt::Orientation direction;
};

} // namespace KWin

#endif

