/*
    SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "auroraeconfig.h"
#include "auroraeshared.h"
#include <KDecoration2/DecorationButton>
#include <KLocalizedString>
#include <KLocalizedTranslator>
#include <KPluginFactory>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QUiLoader>

K_PLUGIN_CLASS(Aurorae::ConfigurationModule)

namespace Aurorae
{

ConfigurationModule::ConfigurationModule(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data)
    , m_theme(findTheme(args))
    , m_buttonSize(int(KDecoration2::BorderSize::Normal) - s_indexMapper)
{
    init();
}

void ConfigurationModule::init()
{
    if (m_theme.startsWith(QLatin1String("__aurorae__svg__"))) {
        // load the generic setting module
        initSvg();
    } else {
        initQml();
    }
}

void ConfigurationModule::initSvg()
{
    QWidget *form = new QWidget(widget());
    form->setLayout(new QHBoxLayout(form));
    QComboBox *sizes = new QComboBox(form);
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Tiny"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Normal"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Large"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Very Large"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Huge"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Very Huge"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Oversized"));
    sizes->setObjectName(QStringLiteral("kcfg_ButtonSize"));

    QLabel *label = new QLabel(i18n("Button size:"), form);
    label->setBuddy(sizes);
    form->layout()->addWidget(label);
    form->layout()->addWidget(sizes);

    widget()->layout()->addWidget(form);

    KCoreConfigSkeleton *skel = new KCoreConfigSkeleton(KSharedConfig::openConfig(QStringLiteral("auroraerc")), this);
    skel->setCurrentGroup(m_theme.mid(16));
    skel->addItemInt(QStringLiteral("ButtonSize"),
                     m_buttonSize,
                     int(KDecoration2::BorderSize::Normal) - s_indexMapper,
                     QStringLiteral("ButtonSize"));
    addConfig(skel, form);
}

void ConfigurationModule::initQml()
{
    const QString packageRoot = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                       QLatin1String("kwin/decorations/") + m_theme,
                                                       QStandardPaths::LocateDirectory);
    if (packageRoot.isEmpty()) {
        return;
    }

    const KPluginMetaData metaData = KPluginMetaData::fromJsonFile(packageRoot + QLatin1String("/metadata.json"));
    if (!metaData.isValid()) {
        return;
    }

    const QString xml = packageRoot + QLatin1String("/contents/config/main.xml");
    const QString ui = packageRoot + QLatin1String("/contents/ui/config.ui");
    if (!QFileInfo::exists(xml) || !QFileInfo::exists(ui)) {
        return;
    }

    KLocalizedTranslator *translator = new KLocalizedTranslator(this);
    QCoreApplication::instance()->installTranslator(translator);
    const QString translationDomain = metaData.value("X-KWin-Config-TranslationDomain");
    if (!translationDomain.isEmpty()) {
        translator->setTranslationDomain(translationDomain);
    }

    // load the KConfigSkeleton
    QFile configFile(xml);
    KSharedConfigPtr auroraeConfig = KSharedConfig::openConfig("auroraerc");
    KConfigGroup configGroup = auroraeConfig->group(m_theme);
    m_skeleton = new KConfigLoader(configGroup, &configFile, this);
    // load the ui file
    QUiLoader *loader = new QUiLoader(this);
    loader->setLanguageChangeEnabled(true);
    QFile uiFile(ui);
    uiFile.open(QFile::ReadOnly);
    QWidget *customConfigForm = loader->load(&uiFile, widget());
    translator->addContextToMonitor(customConfigForm->objectName());
    uiFile.close();
    widget()->layout()->addWidget(customConfigForm);
    // connect the ui file with the skeleton
    addConfig(m_skeleton, customConfigForm);

    // send a custom event to the translator to retranslate using our translator
    QEvent le(QEvent::LanguageChange);
    QCoreApplication::sendEvent(customConfigForm, &le);
}

}

#include "auroraeconfig.moc"
