#ifndef GRAB_FILTER_H
#define GRAB_FILTER_H

#include "input.h"
#include <KWaylandServer/grab.h>

namespace KWin
{

class GrabFilter : public InputEventFilter
{

public:

    GrabFilter();

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(QWheelEvent *event) override;
    bool keyEvent(QKeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

};


}

#endif
