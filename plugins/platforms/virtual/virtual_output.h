/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2018 Roman Gilg <subdiff@gmail.com>

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
#ifndef KWIN_VIRTUAL_OUTPUT_H
#define KWIN_VIRTUAL_OUTPUT_H

#include <QObject>
#include <QRect>

namespace KWin
{
class VirtualBackend;

class VirtualOutput : public QObject
{
    Q_OBJECT

public:
    VirtualOutput(QObject *parent = nullptr);
    virtual ~VirtualOutput();

    QRect geometry() const {
        return m_geo;
    }

private:
    Q_DISABLE_COPY(VirtualOutput);
    friend class VirtualBackend;

    QRect m_geo;
    qreal m_outputScale = 1;

    int m_gammaSize = 200;
    bool m_gammaResult = true;
};

}

#endif
