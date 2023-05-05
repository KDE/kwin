/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWaylandServer
{

class CursorShapeManagerV1InterfacePrivate;
class Display;

class KWIN_EXPORT CursorShapeManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit CursorShapeManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~CursorShapeManagerV1Interface() override;

private:
    std::unique_ptr<CursorShapeManagerV1InterfacePrivate> d;
};

} // namespace KWaylandServer
