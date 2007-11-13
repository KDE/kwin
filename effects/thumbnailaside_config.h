/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_THUMBNAILASIDE_CONFIG_H
#define KWIN_THUMBNAILASIDE_CONFIG_H

#define KDE3_SUPPORT
#include <kcmodule.h>
#undef KDE3_SUPPORT

#include "ui_thumbnailaside_config.h"

class KActionCollection;

namespace KWin
{

class ThumbnailAsideEffectConfigForm : public QWidget, public Ui::ThumbnailAsideEffectConfigForm
{
    Q_OBJECT
    public:
        explicit ThumbnailAsideEffectConfigForm(QWidget* parent);
};

class ThumbnailAsideEffectConfig : public KCModule
    {
    Q_OBJECT
    public:
      explicit ThumbnailAsideEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

        virtual void save();
        virtual void load();
        virtual void defaults();

    private:
        ThumbnailAsideEffectConfigForm* m_ui;
        KActionCollection* m_actionCollection;
    };

} // namespace

#endif
