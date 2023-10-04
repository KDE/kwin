/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QString>
#include <memory>

typedef void *cmsHPROFILE;

namespace KWin
{

class ColorTransformation;

class KWIN_EXPORT IccProfile
{
public:
    explicit IccProfile(cmsHPROFILE handle, const std::shared_ptr<ColorTransformation> &vcgt);
    ~IccProfile();

    std::shared_ptr<ColorTransformation> vcgt() const;

    static std::unique_ptr<IccProfile> load(const QString &path);

private:
    cmsHPROFILE const m_handle;
    const std::shared_ptr<ColorTransformation> m_vcgt;
};

}
