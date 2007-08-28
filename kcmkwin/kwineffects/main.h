/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>

#include <ksharedconfig.h>
#include <QHash>
#include <QSet>

class KPluginSelector;

namespace KWin
{

class KWinEffectsConfig : public KCModule
    {
    Q_OBJECT
    public:
        KWinEffectsConfig(QWidget *parent, const QVariantList &args);
        virtual ~KWinEffectsConfig();

        virtual QString quickHelp() const;

    public slots:
        virtual void load();
        virtual void save();
        virtual void defaults();
	void reparseConfiguration(const QByteArray&conf);

    private:
        KSharedConfigPtr mKWinConfig;
        KPluginSelector* mPluginSelector;
    };

} // namespace

#endif
