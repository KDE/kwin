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

class PopupInputFilter : public QObject, public InputEventFilter
{
    Q_OBJECT
public:
    explicit PopupInputFilter();
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool keyEvent(QKeyEvent *event) override;
private:
    void handleClientAdded(AbstractClient *client);
    void handleClientRemoved(AbstractClient *client);
    void disconnectClient(AbstractClient *client);
    void cancelPopups();

    QVector<AbstractClient*> m_popupClients;
};
}

#endif
