/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "xwaylandeiscontext.h"

#include "options.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <unistd.h>

namespace KWin
{

XWaylandEisContext::XWaylandEisContext(EisBackend *backend)
    : EisContext(backend, {EIS_DEVICE_CAP_POINTER | EIS_DEVICE_CAP_POINTER_ABSOLUTE | EIS_DEVICE_CAP_KEYBOARD | EIS_DEVICE_CAP_TOUCH | EIS_DEVICE_CAP_SCROLL | EIS_DEVICE_CAP_BUTTON})
    , socketName(qgetenv("XDG_RUNTIME_DIR") + QByteArrayLiteral("/kwin-xwayland-eis-socket.") + QByteArray::number(getpid()))
{
    eis_setup_backend_socket(m_eisContext, socketName.constData());
}

void XWaylandEisContext::connectionRequested(eis_client *client)
{
    const QString clientName = QString::fromUtf8(eis_client_get_name(client));
    if (options->xwaylandEisNoPrompt() || options->xwaylandEisNoPromptApps().contains(clientName)) {
        connectToClient(client);
        return;
    }

    auto dialog = new QDialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window", "Remote control requested"));
    dialog->setWindowIcon(QIcon::fromTheme(QStringLiteral("krfb")));
    auto mainLayout = new QVBoxLayout(dialog);
    auto iconTextLayout = new QHBoxLayout();
    mainLayout->addLayout(iconTextLayout);
    const int iconSize = dialog->style()->pixelMetric(QStyle::PM_MessageBoxIconSize);
    auto icon = new QLabel(dialog);
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("krfb")).pixmap(iconSize));
    iconTextLayout->addWidget(icon);
    iconTextLayout->addWidget(new QLabel(i18nc("%1 is the app/binary", "%1 wants to control the pointer and keyboard", eis_client_get_name(client)), dialog));
    auto allowAppCheckbox = new QCheckBox(i18nc("@option:check %1 is the app/binary", "Always allow for %1", clientName), dialog);
    mainLayout->addWidget(allowAppCheckbox);
    auto alwaysAllowCheckbox = new QCheckBox(i18nc("@option:check", "Always allow for legacy applications"), dialog);
    mainLayout->addWidget(alwaysAllowCheckbox);
    auto buttonBox = new QDialogButtonBox(dialog);
    mainLayout->addWidget(buttonBox);
    auto allowButton = buttonBox->addButton(i18nc("@action:button", "Allow"), QDialogButtonBox::AcceptRole);
    allowButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));
    buttonBox->addButton(QDialogButtonBox::Cancel);
    dialog->show();
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(dialog, &QDialog::finished, [client, clientName, alwaysAllowCheckbox, allowAppCheckbox, this](int result) {
        if (result == QDialog::Accepted) {
            connectToClient(client);
            auto xwaylandGroup = kwinApp()->config()->group(QStringLiteral("Xwayland"));
            if (alwaysAllowCheckbox->isChecked()) {
                xwaylandGroup.writeEntry(QStringLiteral("XwaylandEisNoPrompt"), true, KConfig::Notify);
            }
            if (allowAppCheckbox->isChecked()) {
                auto allowedApps = options->xwaylandEisNoPromptApps() << clientName;
                xwaylandGroup.writeEntry(QStringLiteral("XwaylandEisNoPromptApps"), allowedApps, KConfig::Notify);
            }
            kwinApp()->config()->sync();
        } else {
            eis_client_disconnect(client);
        }
    });
    dialog->show();
}

}
