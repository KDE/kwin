/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_SCREENS_VIRTUAL_H
#define KWIN_SCREENS_VIRTUAL_H
#include "outputscreens.h"
#include <QVector>

namespace KWin
{
class VirtualBackend;

class VirtualScreens : public OutputScreens
{
    Q_OBJECT
public:
    VirtualScreens(VirtualBackend *backend, QObject *parent = nullptr);
    virtual ~VirtualScreens();
    void init() override;

private:
    void createOutputs();
    VirtualBackend *m_backend;
};

}

#endif

