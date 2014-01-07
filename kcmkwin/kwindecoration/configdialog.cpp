/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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
#include "configdialog.h"

#include <QDBusConnection>
#include <QDBusMessage>

#include <KLibrary>
#include <KSharedConfig>
#include <KLocalizedString>
#include <KConfigGroup>
#include <QDialogButtonBox>
#include <QPushButton>

namespace KWin
{

static const char* const border_names[ KDecorationDefines::BordersCount ] = {
    I18N_NOOP2("@item:inlistbox Border size:", "Tiny"),
    I18N_NOOP2("@item:inlistbox Border size:", "Normal"),
    I18N_NOOP2("@item:inlistbox Border size:", "Large"),
    I18N_NOOP2("@item:inlistbox Border size:", "Very Large"),
    I18N_NOOP2("@item:inlistbox Border size:", "Huge"),
    I18N_NOOP2("@item:inlistbox Border size:", "Very Huge"),
    I18N_NOOP2("@item:inlistbox Border size:", "Oversized"),
    I18N_NOOP2("@item:inlistbox Border size:", "No Side Border"),
    I18N_NOOP2("@item:inlistbox Border size:", "No Border"),
};

KWinAuroraeConfigForm::KWinAuroraeConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
    connect(borderSizesCombo, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()));
    connect(buttonSizesCombo, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()));
    connect(closeWindowsDoubleClick, SIGNAL(clicked()), SIGNAL(changed()));
}

void KWinAuroraeConfigForm::enableNoSideBorderSupport(bool enable)
{
    if (!enable) {
        return;
    }
    borderSizesCombo->addItem(i18nc("@item:inlistbox Border size:", border_names[KDecorationDefines::BorderNoSides]));
    borderSizesCombo->addItem(i18nc("@item:inlistbox Border size:", border_names[KDecorationDefines::BorderNone]));
}

KWinDecorationConfigForm::KWinDecorationConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KWinDecorationConfigDialog::KWinDecorationConfigDialog(QString deco, const QList<QVariant>& borderSizes,
        KDecorationDefines::BorderSize size,
        QWidget* parent, Qt::WFlags flags)
    : QDialog(parent, flags)
    , m_borderSizes(borderSizes)
    , m_kwinConfig(KSharedConfig::openConfig("kwinrc"))
    , m_pluginObject(nullptr)
    , m_pluginConfigWidget(nullptr)
{
    m_ui = new KWinDecorationConfigForm(this);
    setWindowTitle(i18n("Decoration Options"));

    m_buttons = new QDialogButtonBox(this);
    m_buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Reset);
    m_buttons->button(QDialogButtonBox::Reset)->setEnabled(false);
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(m_ui);
    layout->addWidget(m_buttons);
    connect(m_buttons, SIGNAL(accepted()), SLOT(accept()));
    connect(m_buttons, SIGNAL(rejected()), SLOT(reject()));

    KLibrary library(styleToConfigLib(deco));
    if (library.load()) {
        KLibrary::void_function_ptr alloc_ptr = library.resolveFunction("allocate_config");
        if (alloc_ptr != nullptr) {
            allocatePlugin = (QObject * (*)(KConfigGroup & conf, QWidget * parent))alloc_ptr;
            KConfigGroup config(m_kwinConfig, "Style");
            m_pluginConfigWidget = new QWidget(this);
            m_pluginConfigWidget->setLayout(new QVBoxLayout);
            m_pluginObject = (QObject*)(allocatePlugin(config, m_pluginConfigWidget));

            // connect required signals and slots together...
            connect(this, SIGNAL(accepted()), this, SLOT(slotAccepted()));
            connect(m_pluginObject, SIGNAL(changed()), this, SLOT(slotSelectionChanged()));
            connect(this, SIGNAL(pluginSave(KConfigGroup&)), m_pluginObject, SLOT(save(KConfigGroup&)));
            connect(m_buttons->button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked(bool)), m_pluginObject, SLOT(defaults()));
            connect(m_buttons->button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked(bool)), SLOT(slotDefault()));
        }
    }

    if (m_pluginConfigWidget) {
        layout->addWidget(m_pluginConfigWidget);
    }

    if (borderSizes.count() >= 2) {
        foreach (const QVariant & borderSize, borderSizes) {
            KDecorationDefines::BorderSize currentSize =
                static_cast<KDecorationDefines::BorderSize>(borderSize.toInt());
            m_ui->bordersCombo->addItem(i18nc("@item:inlistbox Border size:", border_names[currentSize]), borderSizeToIndex(currentSize, borderSizes));
        }
        m_ui->bordersCombo->setCurrentIndex(borderSizeToIndex(size, borderSizes));
    } else {
        m_ui->bordersCombo->hide();
        m_ui->borderLabel->hide();
    }

    setLayout(layout);
}

KWinDecorationConfigDialog::~KWinDecorationConfigDialog()
{
    delete m_pluginObject;
}

KDecorationDefines::BorderSize KWinDecorationConfigDialog::borderSize() const
{
    if (m_borderSizes.count() >= 2)
        return (KDecorationDefines::BorderSize)m_borderSizes.at(m_ui->bordersCombo->currentIndex()).toInt();
    return KDecorationDefines::BorderNormal;
}

int KWinDecorationConfigDialog::borderSizeToIndex(KDecorationDefines::BorderSize size, const QList< QVariant >& sizes)
{
    int pos = 0;
    for (QList< QVariant >::ConstIterator it = sizes.constBegin();
            it != sizes.constEnd();
            ++it, ++pos)
        if (size <= (*it).toInt())
            break;
    return pos;
}

void KWinDecorationConfigDialog::slotAccepted()
{
    KConfigGroup config(m_kwinConfig, "Style");
    emit pluginSave(config);
    config.sync();
}

void KWinDecorationConfigDialog::slotSelectionChanged()
{
    m_buttons->button(QDialogButtonBox::Reset)->setEnabled(true);
}

QString KWinDecorationConfigDialog::styleToConfigLib(const QString& styleLib) const
{
    if (styleLib.startsWith(QLatin1String("kwin3_")))
        return "kwin_" + styleLib.mid(6) + "_config";
    else
        return styleLib + "_config";
}

void KWinDecorationConfigDialog::slotDefault()
{
    if (m_borderSizes.count() >= 2)
        m_ui->bordersCombo->setCurrentIndex(borderSizeToIndex(BorderNormal, m_borderSizes));
}

} // namespace KWin

#include "configdialog.moc"
