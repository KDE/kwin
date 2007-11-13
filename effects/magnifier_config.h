/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_MAGNIFIER_CONFIG_H
#define KWIN_MAGNIFIER_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

#include "ui_magnifier_config.h"

class KActionCollection;

namespace KWin
{

class MagnifierEffectConfigForm : public QWidget, public Ui::MagnifierEffectConfigForm
{
    Q_OBJECT
    public:
        explicit MagnifierEffectConfigForm(QWidget* parent);
};

class MagnifierEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
        explicit MagnifierEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        MagnifierEffectConfigForm* m_ui;
        KActionCollection* m_actionCollection;
    };

} // namespace

#endif
