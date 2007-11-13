/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_LOOKINGGLASS_CONFIG_H
#define KWIN_LOOKINGGLASS_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

#include "ui_lookingglass_config.h"

class KActionCollection;

namespace KWin
{

class LookingGlassEffectConfigForm : public QWidget, public Ui::LookingGlassEffectConfigForm
{
    Q_OBJECT
    public:
        explicit LookingGlassEffectConfigForm(QWidget* parent);
};

class LookingGlassEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit LookingGlassEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        LookingGlassEffectConfigForm* m_ui;
        KActionCollection* m_actionCollection;
    };

} // namespace

#endif
