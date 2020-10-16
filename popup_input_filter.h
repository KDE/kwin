/*
    SPDX-FileCopyrightText: 2017 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/
#ifndef KWIN_POPUP_INPUT_FILTER
#define KWIN_POPUP_INPUT_FILTER

#include "input.h"

#include <QObject>
#include <QVector>

namespace KWin
{
class Toplevel;

class PopupInputFilter : public QObject, public InputEventFilter
{
    Q_OBJECT
public:
    explicit PopupInputFilter();
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
private:
    void handleClientAdded(Toplevel *client);
    void handleClientRemoved(Toplevel *client);
    void disconnectClient(Toplevel *client);
    void cancelPopups();

    QVector<Toplevel*> m_popupClients;
};
}

#endif
