#pragma once

#include <QObject>

#include <memory>

namespace KWaylandServer
{
class Display;
class SeatInterface;
class XdgToplevelInterface;
class XdgWmGesturesV1InterfacePrivate;

class XdgWmGesturesV1Interface : public QObject
{
    Q_OBJECT
public:
    enum class Location {
        Titlebar,
        Content,
    };
    enum class Action {
        DoubleClick,
        RightClick,
        MiddleClick
    };
    XdgWmGesturesV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgWmGesturesV1Interface() override;

    struct GestureAction
    {
        quint32 serial;
        SeatInterface *seat;
        XdgToplevelInterface *toplevel;
        Location location;
        Action action;
    };
Q_SIGNALS:
    void action(const GestureAction &action);

private:
    std::unique_ptr<XdgWmGesturesV1InterfacePrivate> d;
};
}
