/**************************************************************************
 * KWin - the KDE window manager                                          *
 * This file is part of the KDE project.                                  *
 *                                                                        *
 * Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 2 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/


#ifndef COMPOSITING_H
#define COMPOSITING_H
#include <QObject>

namespace KWin {
namespace Compositing {

class Compositing : public QObject
{

    Q_OBJECT

public:
    explicit Compositing(QObject *parent = 0);

    Q_INVOKABLE bool OpenGLIsUnsafe();
    Q_INVOKABLE bool OpenGLIsBroken();
    Q_INVOKABLE void syncConfig(int openGLType, int graphicsSystem);
    Q_INVOKABLE int currentOpenGLType();

private:

    enum OpenGLTypeIndex {
        OPENGL31_INDEX = 0,
        OPENGL20_INDEX,
        OPENGL12_INDEX,
        XRENDER_INDEX
    };
};
}//end namespace Compositing
}//end namespace KWin
#endif
