/*
 * SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>
 *
 * SPDX-License-Identifier: MIT
 */

#include "config-kwin.h"

#include <QDialog>

#include "ui_killdialog.h"

class QPushButton;

class KillDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KillDialog(const QString &applicationName, const QIcon &applicationIcon, QWidget *parent = nullptr);

    void setPid(pid_t pid);
    void setHostName(const QString &hostName);
    void setBusy(bool busy);

Q_SIGNALS:
    void terminateRequested();

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui_KillDialog m_ui;

    QString m_question;
    QPushButton *m_terminateButton;
    QPushButton *m_waitButton;
};
