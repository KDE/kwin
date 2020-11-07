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

class AbstractOutput;
class ColordDevice;

class KWIN_EXPORT ColordIntegration : public Plugin
{
    Q_OBJECT

public:
    explicit ColordIntegration(QObject *parent = nullptr);

private Q_SLOTS:
    void handleOutputAdded(AbstractOutput *output);
    void handleOutputRemoved(AbstractOutput *output);

private:
    QHash<AbstractOutput *, ColordDevice *> m_outputToDevice;
    CdInterface *m_colordInterface;
};

} // namespace KWin
