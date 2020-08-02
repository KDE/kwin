/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "abstract_opengl_context_attribute_builder.h"

namespace KWin
{

class GlxContextAttributeBuilder : public AbstractOpenGLContextAttributeBuilder
{
public:
    std::vector<int> build() const override;
};

}
