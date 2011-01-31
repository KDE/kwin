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
#ifndef KWINDECORATIONCONFIGDIALOG_H
#define KWINDECORATIONCONFIGDIALOG_H
#include <QWidget>
#include <kdialog.h>
#include <kdecoration.h>
#include "ui_config.h"
#include "ui_auroraeconfig.h"

namespace KWin
{

class KWinAuroraeConfigForm : public QWidget, public Ui::KWinAuroraeConfigForm
{
    Q_OBJECT

public:
    explicit KWinAuroraeConfigForm(QWidget* parent);
};

class KWinDecorationConfigForm : public QWidget, public Ui::KWinDecorationConfigForm
{
    Q_OBJECT

public:
    explicit KWinDecorationConfigForm(QWidget* parent);
};

class KWinDecorationConfigDialog : public KDialog, public KDecorationDefines
{
    Q_OBJECT
public:
    KWinDecorationConfigDialog(QString decoLib, const QList<QVariant>& borderSizes,
                               KDecorationDefines::BorderSize size, QWidget* parent = 0,
                               Qt::WFlags flags = 0);
    ~KWinDecorationConfigDialog();

    KDecorationDefines::BorderSize borderSize() const;

signals:
    void pluginSave(KConfigGroup& group);

private slots:
    void slotSelectionChanged();
    void slotAccepted();
    void slotDefault();

private:
    int borderSizeToIndex(KDecorationDefines::BorderSize size, const QList<QVariant>& sizes);
    QString styleToConfigLib(const QString& styleLib) const;

private:
    KWinDecorationConfigForm* m_ui;
    QList<QVariant> m_borderSizes;
    KSharedConfigPtr m_kwinConfig;

    QObject*(*allocatePlugin)(KConfigGroup& conf, QWidget* parent);
    QObject* m_pluginObject;
    QWidget* m_pluginConfigWidget;
};

} // namespace KWin

#endif // KWINDECORATIONCONFIGDIALOG_H
