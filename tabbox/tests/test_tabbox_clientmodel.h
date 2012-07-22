/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef TEST_TABBOX_CLIENT_MODEL_H
#define TEST_TABBOX_CLIENT_MODEL_H
#include <QObject>

class TestTabBoxClientModel : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Tests that calculating the longest caption does not
     * crash in case the internal m_clientList contains a weak
     * pointer to a deleted TabBoxClient.
     *
     * See bug #303840
     **/
    void testLongestCaptionWithNullClient();
};

#endif
