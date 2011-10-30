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
// own
#include "layoutconfig.h"
#include "ui_layoutconfig.h"
// tabbox
#include "tabboxconfig.h"

namespace KWin
{
namespace TabBox
{

/***************************************************
* LayoutConfigPrivate
***************************************************/
class LayoutConfigPrivate
{
public:
    LayoutConfigPrivate();
    ~LayoutConfigPrivate() { }

    TabBoxConfig config;
    Ui::LayoutConfigForm ui;
};

LayoutConfigPrivate::LayoutConfigPrivate()
{
}

/***************************************************
* LayoutConfig
***************************************************/
LayoutConfig::LayoutConfig(QWidget* parent)
    : QWidget(parent)
{
    d = new LayoutConfigPrivate;
    d->ui.setupUi(this);

    // init the item layout combo box
    d->ui.itemLayoutCombo->addItem(i18n("Informative"));
    d->ui.itemLayoutCombo->addItem(i18n("Compact"));
    d->ui.itemLayoutCombo->addItem(i18n("Small Icons"));
    d->ui.itemLayoutCombo->addItem(i18n("Large Icons"));
    d->ui.itemLayoutCombo->addItem(i18n("Text Only"));
    // TODO: user defined layouts

    connect(d->ui.itemLayoutCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
}

LayoutConfig::~LayoutConfig()
{
    delete d;
}

TabBoxConfig& LayoutConfig::config() const
{
    return d->config;
}

void LayoutConfig::setConfig(const KWin::TabBox::TabBoxConfig& config)
{
    d->config = config;
    // item layouts
    if (config.layoutName().compare("Default", Qt::CaseInsensitive) == 0) {
        d->ui.itemLayoutCombo->setCurrentIndex(0);
    } else if (config.layoutName().compare("Compact", Qt::CaseInsensitive) == 0) {
        d->ui.itemLayoutCombo->setCurrentIndex(1);
    } else if (config.layoutName().compare("Small Icons", Qt::CaseInsensitive) == 0) {
        d->ui.itemLayoutCombo->setCurrentIndex(2);
    } else if (config.layoutName().compare("Big Icons", Qt::CaseInsensitive) == 0) {
        d->ui.itemLayoutCombo->setCurrentIndex(3);
    } else if (config.layoutName().compare("Text", Qt::CaseInsensitive) == 0) {
        d->ui.itemLayoutCombo->setCurrentIndex(4);
    } else {
        // TODO: user defined layouts
    }

}

void LayoutConfig::changed()
{
    QString layout;
    switch(d->ui.itemLayoutCombo->currentIndex()) {
    case 0:
        layout = "Default";
        break;
    case 1:
        layout = "Compact";
        break;
    case 2:
        layout = "Small Icons";
        break;
    case 3:
        layout = "Big Icons";
        break;
    case 4:
        layout = "Text";
        break;
    default:
        // TODO: user defined layouts
        break;
    }
    d->config.setLayoutName(layout);
}

} // namespace KWin
} // namespace TabBox

