/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QKeySequence>

#include <KConfigSkeleton>

class KActionCollection;

namespace KWin
{
namespace TabBox
{

class ShortcutSettings : public KConfigSkeleton
{
public:
    explicit ShortcutSettings(QObject *parent);

    KActionCollection *actionCollection() const;

private:
    KActionCollection *m_actionCollection = nullptr;
};

} // namespace TabBox
} // namespace KWin
