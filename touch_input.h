/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_TOUCH_INPUT_H
#define KWIN_TOUCH_INPUT_H
#include "input.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QPointF>

namespace KWin
{

class InputRedirection;
class Toplevel;

namespace Decoration
{
class DecoratedClientImpl;
}

class TouchInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit TouchInputRedirection(InputRedirection *parent);
    virtual ~TouchInputRedirection();

    void update(const QPointF &pos = QPointF());
    void init();

    void processDown(qint32 id, const QPointF &pos, quint32 time);
    void processUp(qint32 id, quint32 time);
    void processMotion(qint32 id, const QPointF &pos, quint32 time);
    void cancel();
    void frame();

    void insertId(quint32 internalId, qint32 kwaylandId);
    void removeId(quint32 internalId);
    qint32 mappedId(quint32 internalId);

    void setDecorationPressId(qint32 id) {
        m_decorationId = id;
    }
    qint32 decorationPressId() const {
        return m_decorationId;
    }

private:
    bool m_inited = false;
    qint32 m_decorationId = -1;
    /**
     * external/kwayland
     **/
    QHash<qint32, qint32> m_idMapper;
    QMetaObject::Connection m_windowGeometryConnection;
};

}

#endif
