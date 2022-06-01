/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

namespace KWin
{

struct CDeleter
{
    template<typename T>
    void operator()(T *ptr)
    {
        if (ptr) {
            free(ptr);
        }
    }
};

template<typename T>
using UniqueCPtr = std::unique_ptr<T, CDeleter>;

}
