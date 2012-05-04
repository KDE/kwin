/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>

#include <ksharedconfig.h>
#include <ktemporaryfile.h>

#include "kwin_interface.h"

#include "ui_main.h"
#include "ktimerdialog.h"

class KPluginSelector;
class KActionCollection;
class QLabel;

namespace KWin
{

class ConfirmDialog : public KTimerDialog
{
    Q_OBJECT
public:
    ConfirmDialog();
};

class KWinCompositingConfig : public KCModule
{
    Q_OBJECT
public:
    KWinCompositingConfig(QWidget *parent, const QVariantList &args);
    virtual ~KWinCompositingConfig();

    virtual QString quickHelp() const;

public slots:
    virtual void showConfirmDialog(bool reinitCompositing);
    void currentTabChanged(int tab);

    virtual void load();
    virtual void save();
    virtual void defaults();
    void reparseConfiguration(const QByteArray& conf);

    void loadGeneralTab();
    void loadEffectsTab();
    void loadAdvancedTab();
    void saveGeneralTab();
    void saveEffectsTab();
    bool saveAdvancedTab();

    void checkLoadedEffects();
    void configChanged(bool reinitCompositing);
    void initEffectSelector();

private slots:
    void confirmReInit() { showConfirmDialog(true); }
    void rearmGlSupport();
    void suggestGraphicsSystem();
    void toogleSmoothScaleUi(int compositingType);
    void toggleEffectShortcutChanged(const QKeySequence &seq);
    void updateStatusUI(bool compositingIsPossible);
    void showDetailedEffectLoadingInformation();

private:
    bool effectEnabled(const QString& effect, const KConfigGroup& cfg) const;

    KSharedConfigPtr mKWinConfig;
    Ui::KWinCompositingConfig ui;

    QMap<QString, QString> mPreviousConfig;
    KTemporaryFile mTmpConfigFile;
    KSharedConfigPtr mTmpConfig;
    bool m_showConfirmDialog;
    KActionCollection* m_actionCollection;
    QString originalGraphicsSystem;
    QAction *m_showDetailedErrors;
};

} // namespace

#endif
