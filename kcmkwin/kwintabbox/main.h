/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "tabboxconfig.h"

#include "ui_main.h"

class KShortcutsEditor;
class KActionCollection;

namespace KWin
{
namespace TabBox
{

class LayoutPreview;
}



class KWinTabBoxConfigForm : public QWidget, public Ui::KWinTabBoxConfigForm
{
    Q_OBJECT

public:
    explicit KWinTabBoxConfigForm(QWidget* parent);
};

class KWinTabBoxConfig : public KCModule
{
    Q_OBJECT

public:
    explicit KWinTabBoxConfig(QWidget* parent, const QVariantList& args);
    ~KWinTabBoxConfig();

public slots:
    virtual void save();
    virtual void load();
    virtual void defaults();

private slots:
    void effectSelectionChanged(int index);
    void configureEffectClicked();
    void tabBoxToggled(bool on);
    void shortcutChanged(const QKeySequence &seq);
private:
    void updateUiFromConfig(KWinTabBoxConfigForm* ui, const TabBox::TabBoxConfig& config);
    void updateConfigFromUi(const KWinTabBoxConfigForm* ui, TabBox::TabBoxConfig& config);
    void loadConfig(const KConfigGroup& config, KWin::TabBox::TabBoxConfig& tabBoxConfig);
    void saveConfig(KConfigGroup& config, const KWin::TabBox::TabBoxConfig& tabBoxConfig);

private:
    enum Mode {
        CoverSwitch = 0,
        FlipSwitch = 1,
        Layout = 2
    };
    KWinTabBoxConfigForm* m_primaryTabBoxUi;
    KWinTabBoxConfigForm* m_alternativeTabBoxUi;
    KSharedConfigPtr m_config;
    KActionCollection* m_actionCollection;
    KShortcutsEditor* m_editor;
    TabBox::TabBoxConfig m_tabBoxConfig;
    TabBox::TabBoxConfig m_tabBoxAlternativeConfig;
    TabBox::LayoutPreview *m_layoutPreview;

    bool effectEnabled(const QString& effect, const KConfigGroup& cfg) const;
};

} // namespace

#endif
