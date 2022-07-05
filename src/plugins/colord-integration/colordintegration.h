/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "colordinterface.h"
#include "plugin.h"

#include <QHash>
#include <QObject>

namespace KWin
{

class Output;
class ColordDevice;

class KWIN_EXPORT ColordIntegration : public Plugin
{
    Q_OBJECT

public:
    explicit ColordIntegration();

private Q_SLOTS:
    void handleOutputAdded(Output *output);
    void handleOutputRemoved(Output *output);

private:
    void initialize();
    void teardown();

    QHash<Output *, ColordDevice *> m_outputToDevice;
    CdInterface *m_colordInterface;
};

} // namespace KWin
