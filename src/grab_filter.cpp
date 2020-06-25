#include "grab_filter.h"
#include "input.h"
#include "wayland_server.h"

#include <KWaylandServer/grabs.h>
#include <KWaylandServer/seat_interface.h>

#include <QMouseEvent>
#include <qglobal.h>

#define theSeat waylandServer()->seat()

namespace KWin {

GrabFilter::GrabFilter() : InputEventFilter()
{

}

bool GrabFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    if (theSeat->pointerGrab() == nullptr) return false;

    return theSeat->pointerGrab()->pointerEvent(event, nativeButton);
}

bool GrabFilter::wheelEvent(QWheelEvent *event)
{
    if (theSeat->pointerGrab() == nullptr) return false;

    return theSeat->pointerGrab()->wheelEvent(event);
}

bool GrabFilter::keyEvent(QKeyEvent *event)
{
    if (theSeat->keyboardGrab() == nullptr) return false;

    return theSeat->keyboardGrab()->keyEvent(event);
}

bool GrabFilter::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    if (theSeat->touchGrab() == nullptr) return false;

    return theSeat->touchGrab()->touchDown(id, pos, time);
}

bool GrabFilter::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    if (theSeat->touchGrab() == nullptr) return false;

    return theSeat->touchGrab()->touchMotion(id, pos, time);
}

bool GrabFilter::touchUp(qint32 id, quint32 time)
{
    if (theSeat->touchGrab() == nullptr) return false;

    return theSeat->touchGrab()->touchUp(id, time);

}

}
