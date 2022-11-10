/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"

namespace KWin
{

class X11StandaloneBackend;

class X11PlaceholderOutput : public Output
{
    Q_OBJECT

public:
    explicit X11PlaceholderOutput(X11StandaloneBackend *backend, QObject *parent = nullptr);

    RenderLoop *renderLoop() const override;

    void updateEnabled(bool enabled);

private:
    X11StandaloneBackend *m_backend;
};

} // namespace KWin
