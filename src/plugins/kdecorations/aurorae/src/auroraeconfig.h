/*
    SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef AURORAECONFIG_H
#define AURORAECONFIG_H

#include <KCModule>
#include <KConfigLoader>

namespace Aurorae
{

class ConfigurationModule : public KCModule
{
    Q_OBJECT
public:
    ConfigurationModule(QObject *parent, const KPluginMetaData &data, const QVariantList &args);

private:
    void init();
    void initSvg();
    void initQml();
    QString m_theme;
    KConfigLoader *m_skeleton = nullptr;
    int m_buttonSize;
};

}

#endif // AURORAECONFIG_H
