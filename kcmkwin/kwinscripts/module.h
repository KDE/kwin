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

#include <KCModule>
#include <KSharedConfig>

namespace Ui
{
class Module;
}

class KJob;

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
    explicit Module(QWidget *parent, const QVariantList &args = QVariantList());

    /**
     * Destructor.
     */
    ~Module();
    void load() override;
    void save() override;
    void defaults() override;

protected Q_SLOTS:

    /**
     * Called when the import script button is clicked.
     */
    void importScript();

    void importScriptInstallFinished(KJob *job);

private:
    /**
     * UI
     */
    Ui::Module *ui;
    /**
     * Updates the contents of the list view.
     */
    void updateListViewContents();
    KSharedConfigPtr m_kwinConfig;
};

#endif // MODULE_H
