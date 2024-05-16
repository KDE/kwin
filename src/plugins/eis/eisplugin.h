/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

namespace KWin
{
class EisInputCaptureManager;
};

class EisPlugin : public KWin::Plugin
{
    Q_OBJECT
public:
    EisPlugin();
    ~EisPlugin() override;

private:
    std::unique_ptr<KWin::EisInputCaptureManager> m_inputCapture;
};
