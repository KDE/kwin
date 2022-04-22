/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbuffer.h"

#include <QPointer>

namespace KWaylandServer
{
class Display;

class KWIN_EXPORT ClientBufferIntegration : public QObject
{
    Q_OBJECT

public:
    explicit ClientBufferIntegration(Display *display);
    ~ClientBufferIntegration() override;

    Display *display() const;

    virtual ClientBuffer *createBuffer(wl_resource *resource);

private:
    QPointer<Display> m_display;
};

} // namespace KWaylandServer
