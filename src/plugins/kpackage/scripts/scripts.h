/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <KPackage/PackageStructure>

class ScriptsPackage : public KPackage::PackageStructure
{
public:
    ScriptsPackage(QObject *, const QVariantList &)
    {
    }
    void initPackage(KPackage::Package *package) override;
    void pathChanged(KPackage::Package *package) override;
};
