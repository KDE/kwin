/*
 * SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>
 *
 * SPDX-License-Identifier: MIT
 */

#include "killdialog.h"
#include "config-kwin.h"

#include <KIconUtils>
#include <KLocalizedString>
#include <KMessageDialog>

#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QStyle>
#include <QStyleOption>
#include <QWindow>

KillDialog::KillDialog(const QString &applicationName, const QIcon &applicationIcon, QWidget *parent)
    : QDialog(parent)
{
    m_ui.setupUi(this);
    QStyleOption option;
    option.initFrom(this);

    // Drop redundant application name, cf. QXcbWindow::setWindowTitle.
    QString caption = applicationName.toHtmlEscaped();
    const QString titleSeparator = QString::fromUtf8(" \xe2\x80\x94 "); // // U+2014, EM DASH
    caption.remove(titleSeparator + applicationName);
    caption.remove(QStringLiteral(" – ") + applicationName); // EN dash (Firefox)
    caption.remove(QStringLiteral(" - ") + applicationName); // classic minus :-)

    const QString question = (caption == applicationName)
        ? xi18nc("@info",
                 "<para><application>%1</application> is not responding. Do you want to terminate this application?</para>"
                 "<para><emphasis strong='true'>Terminating this application will close all of its windows. Any unsaved data will be lost.</emphasis></para>",
                 applicationName)
        : xi18nc("@info \"window title\" of application name is not responding.",
                 "<para>\"%1\" of <application>%2</application> is not responding. Do you want to terminate this application?</para>"
                 "<para><emphasis strong='true'>Terminating this application will close all of its windows. Any unsaved data will be lost.</emphasis></para>",
                 caption, applicationName);
    m_ui.label_message->setText(question);

    // Extra spacing, matches KMessageDialog.
    m_ui.layout_message->setSpacing(3 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, &option, this));

    QIcon icon;
    if (!applicationIcon.isNull()) {
        // emblem-warning is non-standard, fall back to emblem-important if necessary.
        const QIcon warningBadge = QIcon::fromTheme(QStringLiteral("emblem-warning"), QIcon::fromTheme(QStringLiteral("emblem-important")));

        icon = KIconUtils::addOverlay(applicationIcon, warningBadge, qApp->isRightToLeft() ? Qt::BottomLeftCorner : Qt::BottomRightCorner);
    } else {
        icon = QIcon::fromTheme(QIcon::ThemeIcon::DialogWarning);
    }
    const int iconSize = style()->pixelMetric(QStyle::PM_MessageBoxIconSize, &option, this);
    m_ui.icon->setPixmap(icon.pixmap(iconSize));

    // Enable word wrap when it gets too wide, matches KMessageDialog.
    m_ui.label_message->setWordWrap(m_ui.label_message->sizeHint().width() > screen()->geometry().width() * 0.5);

    m_ui.label_hostname->hide();

    m_ui.label_status->setText(i18n("Terminating %1…", applicationName));
    m_ui.widget_status->hide();

    m_terminateButton = new QPushButton(QIcon::fromTheme(QIcon::ThemeIcon::ApplicationExit), i18nc("@action:button Terminate app", "&Terminate %1", applicationName), this);
    m_ui.buttonBox->addButton(m_terminateButton, QDialogButtonBox::DestructiveRole);

    m_waitButton = new QPushButton(QIcon::fromTheme(QStringLiteral("chronometer")), i18nc("@action:button Wait for frozen app to maybe respond again", "&Wait Longer"), this);
    m_ui.buttonBox->addButton(m_waitButton, QDialogButtonBox::RejectRole);
    m_waitButton->setFocus();

    connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_terminateButton, &QPushButton::clicked, this, &KillDialog::terminateRequested);
}

void KillDialog::setPid(pid_t pid)
{
    // format pid ourself as it does not make sense to format an ID according to locale settings
    const QString pidString = QString::number(pid);
    m_ui.label_pid->setText(i18nc("@info", "Process ID: %1", pidString));
    m_ui.label_pid->show();
}

void KillDialog::setHostName(const QString &hostName)
{
    if (!hostName.isEmpty() && hostName != QLatin1StringView("localhost")) {
        m_ui.label_hostname->setText(i18nc("@info", "Host name: %1", hostName));
        m_ui.label_hostname->show();
    } else {
        m_ui.label_hostname->hide();
    }
}

void KillDialog::setBusy(bool busy)
{
    if (busy) {
        // Hide widgets first before showing the other to avoid the dialog growing.
        m_terminateButton->hide();
        m_ui.widget_status->show();
    } else {
        m_ui.widget_status->hide();
        m_terminateButton->show();
    }
    m_waitButton->setEnabled(!busy);
}

void KillDialog::showEvent(QShowEvent *event)
{
    if (!event->spontaneous()) {
        KMessageDialog::beep(KMessageDialog::WarningContinueCancel, m_ui.label_message->text(), this);
    }
    QDialog::showEvent(event);
}
