/*
 * Copyright (c) 2011 Tamas Krutki <ktamasw@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef MODULE_H
#define MODULE_H

#include <KDE/KCModule>

namespace Ui
{
class Module;
}

class Module : public KCModule
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * @param parent Parent widget of the module
     * @param args Arguments for the module
     */
    Module(QWidget *parent, const QVariantList &args = QVariantList());

    /**
     * Destructor.
     */
    ~Module();

protected slots:
    /**
     * Called when the selection changes in the list view.
     * Disables/enables the remove and export buttons.
     */
    void updateButtons();

    /**
     * Called when the export script button is clicked.
     */
    void exportScript();

    /**
     * Called when the import script button is clicked.
     */
    void importScript();

    /**
     * Called when the remove script button is clicked.
     */
    void removeScript();

private:
    /**
     * UI
     */
    Ui::Module *ui;
    /**
     * Updates the contents of the list view.
     */
    void updateListViewContents();
};

#endif // MODULE_H
