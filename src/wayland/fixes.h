/*
    SPDX-FileCopyrightText: 2024 Joaquim Monteiro <joaquim.monteiro@protonmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "config-kwin.h"
#if HAVE_WL_FIXES

#include "kwin_export.h"

#include <QObject>

namespace KWin
{

class Display;
class FixesInterfacePrivate;

class KWIN_EXPORT FixesInterface : public QObject
{
    Q_OBJECT

public:
    explicit FixesInterface(Display *display, QObject *parent = nullptr);
    ~FixesInterface() override;

private:
    std::unique_ptr<FixesInterfacePrivate> d;
};

} // namespace KWin

#endif // HAVE_WL_FIXES
