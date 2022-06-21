/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "splitscreen.h"

#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include <QDebug>

#include <input.h>
#include <output.h>
#include <platform.h>
#include <waylandoutput.h>
#include <workspace.h>

namespace KWin
{

Splitscreen::Splitscreen(QObject *parent)
    : Plugin(parent)
{
    QAction *toggleAction = new QAction(this);
    toggleAction->setProperty("componentName", QStringLiteral(KWIN_NAME));
    toggleAction->setObjectName(QStringLiteral("Split Active Output"));
    toggleAction->setText(i18n("Split Active Output"));
    KGlobalAccel::setGlobalShortcut(toggleAction, QList<QKeySequence>{{Qt::META | Qt::Key_C}});
    input()->registerShortcut(QKeySequence(), toggleAction, this, &Splitscreen::splitActiveOutput);
    QTimer::singleShot(100, this, &Splitscreen::splitActiveOutput);
}

void Splitscreen::splitActiveOutput()
{
    const auto output = workspace()->activeOutput();
    const auto outputSize = output->geometry().size();

    const QSize splitOutputSize = {outputSize.width() / 2, outputSize.height()};

    auto outputLeft = kwinApp()->platform()->createVirtualOutput(i18n("%1 - Left Side", output->name()), splitOutputSize, output->scale());
    auto outputRight = kwinApp()->platform()->createVirtualOutput(i18n("%1 - Right Side", output->name()), splitOutputSize, output->scale());

    output->setSkip(true);
    outputLeft->moveTo(output->position());
    outputRight->moveTo(output->position() + QPoint{splitOutputSize.width(), 0});

    qDebug() << "splitty split" << outputRight << outputRight->geometry() << output->geometry();
}

} // namespace KWin
