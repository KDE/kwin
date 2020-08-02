/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREENS_DRM_H
#define KWIN_SCREENS_DRM_H
#include "outputscreens.h"

namespace KWin
{
class DrmBackend;

class DrmScreens : public OutputScreens
{
    Q_OBJECT
public:
    DrmScreens(DrmBackend *backend, QObject *parent = nullptr);
    ~DrmScreens() override;

    bool supportsTransformations(int screen) const override;

    DrmBackend *m_backend;
};

}

#endif
