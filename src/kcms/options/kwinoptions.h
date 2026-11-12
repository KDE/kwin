/*
    SPDX-FileCopyrightText: 2026 Tobias Ozór <tobiasozor@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinoptions_settings.h"
#include <KQuickManagedConfigModule>
#include <QKeySequence>

class KWinOptionsSettings;
class QAction;

class KWinOptions : public KQuickManagedConfigModule
{
    Q_OBJECT

    Q_PROPERTY(bool isPiPEnabled READ isPiPEnabled CONSTANT)
    Q_PROPERTY(KWinOptionsSettings *kwinSettings READ kwinSettings CONSTANT)
    Q_PROPERTY(QKeySequence untileActiveScreenShortcut READ untileActiveScreenShortcut WRITE setUntileActiveScreenShortcut NOTIFY untileActiveScreenShortcutChanged)

public:
    KWinOptions(QObject *parent, const KPluginMetaData &data);

    bool isPiPEnabled() const;
    KWinOptionsSettings *kwinSettings() const;

    QKeySequence untileActiveScreenShortcut() const;
    void setUntileActiveScreenShortcut(const QKeySequence &sequence);

public Q_SLOTS:
    void load() override;
    void save() override;

Q_SIGNALS:
    void untileActiveScreenShortcutChanged();

private:
    bool isSaveNeeded() const override;

    KWinOptionsSettings *m_kwinSettings;
    QAction *m_untileActiveScreenAction;
    QKeySequence m_untileActiveScreenShortcutInitialValue;
    QKeySequence m_untileActiveScreenShortcut;
};
