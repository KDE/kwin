/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef WINDOWSWITCHER_H
#define WINDOWSWITCHER_H

#include <KPackage/PackageStructure>

class SwitcherPackage : public KPackage::PackageStructure
{
public:
    SwitcherPackage(QObject*, const QVariantList &) {}
    void initPackage(KPackage::Package *package) override;
    void pathChanged(KPackage::Package *package) override;
};

#endif
