/*
    SPDX-FileCopyrightText: 2017 Demitrius Belai <demitriusbelai@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DECORATIONPACKAGE_H
#define DECORATIONPACKAGE_H

#include <KPackage/PackageStructure>

class DecorationPackage : public KPackage::PackageStructure
{
public:
    DecorationPackage(QObject*, const QVariantList &) {}
    void initPackage(KPackage::Package *package) override;
    void pathChanged(KPackage::Package *package) override;
};

#endif
