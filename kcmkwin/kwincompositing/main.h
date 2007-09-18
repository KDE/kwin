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

#include <QMessageBox>

#include "ui_main.h"
#include "compositingprefs.h"

class KPluginSelector;

namespace KWin
{

class ConfirmDialog : public QMessageBox
{
Q_OBJECT
public:
    ConfirmDialog();

protected slots:
    void advanceTimer();

private:
    int mSecondsToLive;
};

class KWinCompositingConfig : public KCModule
    {
    Q_OBJECT
    public:
        KWinCompositingConfig(QWidget *parent, const QVariantList &args);
        virtual ~KWinCompositingConfig();

        virtual QString quickHelp() const;

    public slots:
        virtual void compositingEnabled(bool enabled);
        virtual void showAdvancedOptions();
        virtual void showConfirmDialog();

        virtual void load();
        virtual void save();
        virtual void defaults();
        void reparseConfiguration(const QByteArray&conf);

        void configChanged();
        void initEffectSelector();

    private:
        KSharedConfigPtr mKWinConfig;
        Ui::KWinCompositingConfig ui;
        CompositingPrefs mDefaultPrefs;

        QMap<QString, QString> mPreviousConfig;
    };

} // namespace

#endif
