/*
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <KPackage/PackageStructure>

class EffectPackageStructure : public KPackage::PackageStructure
{
    Q_OBJECT

public:
    EffectPackageStructure(QObject *parent = nullptr, const QVariantList &args = {});

    void initPackage(KPackage::Package *package) override;
    void pathChanged(KPackage::Package *package) override;
};
