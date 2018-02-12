/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
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

#include "detectwidget.h"
#include "../../plugins/platforms/x11/standalone/x11cursor.h"

#include <KLocalizedString>
#include <QDebug>
#include <kwindowsystem.h>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QEvent>
#include <QByteArray>
#include <QTimer>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

Q_DECLARE_METATYPE(NET::WindowType)

namespace KWin
{

DetectWidget::DetectWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

DetectDialog::DetectDialog(QWidget* parent, const char* name)
    : QDialog(parent)
{
    setObjectName(name);
    setModal(true);
    setLayout(new QVBoxLayout);

    widget = new DetectWidget(this);
    layout()->addWidget(widget);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
    layout()->addWidget(buttons);

    connect(buttons, SIGNAL(accepted()), SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), SLOT(reject()));
}

void DetectDialog::detect(int secs)
{
    QTimer::singleShot(secs*1000, this, SLOT(selectWindow()));
}

void DetectDialog::executeDialog()
{
    static const char* const types[] = {
        I18N_NOOP("Normal Window"),
        I18N_NOOP("Desktop"),
        I18N_NOOP("Dock (panel)"),
        I18N_NOOP("Toolbar"),
        I18N_NOOP("Torn-Off Menu"),
        I18N_NOOP("Dialog Window"),
        I18N_NOOP("Override Type"),
        I18N_NOOP("Standalone Menubar"),
        I18N_NOOP("Utility Window"),
        I18N_NOOP("Splash Screen")
    };
    widget->class_label->setText(wmclass_class + QLatin1String(" (") + wmclass_name + ' ' + wmclass_class + ')');
    widget->role_label->setText(role);
    widget->match_role->setEnabled(!role.isEmpty());
    if (type == NET::Unknown)
        widget->type_label->setText(i18n("Unknown - will be treated as Normal Window"));
    else
        widget->type_label->setText(i18n(types[ type ]));
    widget->title_label->setText(title);
    widget->machine_label->setText(machine);
    widget->adjustSize();
    adjustSize();
    if (width() < 4*height()/3)
        resize(4*height()/3, height());
    emit detectionDone(exec() == QDialog::Accepted);
}

QByteArray DetectDialog::selectedClass() const
{
    if (widget->match_whole_class->isChecked())
        return wmclass_name + ' ' + wmclass_class;
    return wmclass_class;
}

bool DetectDialog::selectedWholeClass() const
{
    return widget->match_whole_class->isChecked();
}

QByteArray DetectDialog::selectedRole() const
{
    if (widget->match_role->isChecked())
        return role;
    return "";
}

QString DetectDialog::selectedTitle() const
{
    return title;
}

Rules::StringMatch DetectDialog::titleMatch() const
{
    return widget->match_title->isChecked() ? Rules::ExactMatch : Rules::UnimportantMatch;
}

bool DetectDialog::selectedWholeApp() const
{
    return !widget->match_type->isChecked();
}

NET::WindowType DetectDialog::selectedType() const
{
    return type;
}

QByteArray DetectDialog::selectedMachine() const
{
    return machine;
}

void DetectDialog::selectWindow()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/KWin"),
                                                          QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("queryWindowInfo"));
    QDBusPendingReply<QVariantMap> async = QDBusConnection::sessionBus().asyncCall(message);

    QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(async, this);
    connect(callWatcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *self) {
            QDBusPendingReply<QVariantMap> reply = *self;
            self->deleteLater();
            if (!reply.isValid()) {
                emit detectionDone(false);
                return;
            }
            m_windowInfo = reply.value();
            wmclass_class = m_windowInfo.value("resourceClass").toByteArray();
            wmclass_name = m_windowInfo.value("resourceName").toByteArray();
            role = m_windowInfo.value("role").toByteArray();
            type = m_windowInfo.value("type").value<NET::WindowType>();
            title = m_windowInfo.value("caption").toString();
            machine = m_windowInfo.value("clientMachine").toByteArray();
            executeDialog();
        }
    );
}

} // namespace

