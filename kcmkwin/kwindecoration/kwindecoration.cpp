/*
    This is the new kwindecoration kcontrol module

    Copyright (c) 2001
        Karol Szwed <gallium@kde.org>
        http://gallium.n3.net/
    Copyright 2009, 2010 Martin Gräßlin <kde@martin-graesslin.com>

    Supports new kwin configuration plugins, and titlebar button position
    modification via dnd interface.

    Based on original "kwintheme" (Window Borders)
    Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
// own
#include "kwindecoration.h"
#include "buttonsconfigdialog.h"
#include "configdialog.h"
#include "decorationmodel.h"
#include "auroraetheme.h"
// Qt
#include <QTimer>
#include <QtDBus/QtDBus>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeItem>
#include <QtDeclarative/QDeclarativeView>
#include <QtGui/QSortFilterProxyModel>
#include <QtGui/QGraphicsObject>
#include <QtGui/QScrollBar>
#include <QUiLoader>
// KDE
#include <KAboutData>
#include <KDialog>
#include <KLocale>
#include <KMessageBox>
#include <KNS3/DownloadDialog>
#include <KDE/KStandardDirs>
#include <KDE/KConfigDialogManager>
#include <KPluginFactory>
#include <Plasma/ConfigLoader>
#include <qdeclarative.h>

// KCModule plugin interface
// =========================
K_PLUGIN_FACTORY(KWinDecoFactory,
                 registerPlugin<KWin::KWinDecorationModule>();
                )
K_EXPORT_PLUGIN(KWinDecoFactory("kcmkwindecoration"))

namespace KWin
{

KWinDecorationForm::KWinDecorationForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}


KWinDecorationModule::KWinDecorationModule(QWidget* parent, const QVariantList &)
    : KCModule(KWinDecoFactory::componentData(), parent)
    , kwinConfig(KSharedConfig::openConfig("kwinrc"))
    , m_showTooltips(false)
    , m_configLoaded(false)
    , m_decorationButtons(new DecorationButtons(this))
    , m_lastPreviewWidth(-1)
    , m_previewUpdateTimer(NULL)
{
    const QString mainQmlPath = KStandardDirs::locate("data", "kwin/kcm_kwindecoration/main.qml");
    if (mainQmlPath.isNull()) {
        // TODO 4.10 i18n this
        KMessageBox::error(this, "<h1>Installation error</h1>"
        "The resource<h2>kwin/kcm_kwindecoration/main.qml</h2>could not be located in any application data path."
        "<h2>Please contact your distribution</h2>"
        "The application will now abort", "Installation Error");
        abort();
    }
    qmlRegisterType<Aurorae::AuroraeTheme>("org.kde.kwin.aurorae", 0, 1, "AuroraeTheme");
    m_ui = new KWinDecorationForm(this);
    m_ui->configureDecorationButton->setIcon(KIcon("configure"));
    m_ui->configureButtonsButton->setIcon(KIcon("configure"));
    m_ui->ghnsButton->setIcon(KIcon("get-hot-new-stuff"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    KConfigGroup style(kwinConfig, "Style");

    // Set up the decoration lists and other UI settings
    m_model = new DecorationModel(kwinConfig, this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_ui->decorationList->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    foreach (const QString &importPath, KGlobal::dirs()->findDirs("module", "imports")) {
        m_ui->decorationList->engine()->addImportPath(importPath);
    }
    m_ui->decorationList->rootContext()->setContextProperty("decorationModel", m_proxyModel);
    m_ui->decorationList->rootContext()->setContextProperty("decorationBaseModel", m_model);
    m_ui->decorationList->rootContext()->setContextProperty("options", m_decorationButtons);
    m_ui->decorationList->rootContext()->setContextProperty("highlightColor", m_ui->decorationList->palette().color(QPalette::Highlight));
    m_ui->decorationList->rootContext()->setContextProperty("sliderWidth", m_ui->decorationList->verticalScrollBar()->width());
    m_ui->decorationList->rootContext()->setContextProperty("auroraeSource", KStandardDirs::locate("data", "kwin/aurorae/aurorae.qml"));
    m_ui->decorationList->rootContext()->setContextProperty("decorationActiveCaptionColor", KDecoration::options()->color(ColorFont, true));
    m_ui->decorationList->rootContext()->setContextProperty("decorationInactiveCaptionColor", KDecoration::options()->color(ColorFont, false));
    m_ui->decorationList->rootContext()->setContextProperty("decorationActiveTitleBarColor", KDecoration::options()->color(ColorTitleBar, true));
    m_ui->decorationList->rootContext()->setContextProperty("decorationInactiveTitleBarColor", KDecoration::options()->color(ColorTitleBar, false));
    m_ui->decorationList->setSource(mainQmlPath);

    readConfig(style);

    connect(m_ui->decorationList->rootObject(), SIGNAL(currentIndexChanged()), SLOT(slotSelectionChanged()));
    connect(m_ui->decorationList->rootObject(), SIGNAL(widthChanged()), SLOT(updatePreviewWidth()));
    connect(m_ui->configureButtonsButton, SIGNAL(clicked(bool)), this, SLOT(slotConfigureButtons()));
    connect(m_ui->ghnsButton, SIGNAL(clicked(bool)), SLOT(slotGHNSClicked()));
    connect(m_ui->searchEdit, SIGNAL(textChanged(QString)), m_proxyModel, SLOT(setFilterFixedString(QString)));
    connect(m_ui->configureDecorationButton, SIGNAL(clicked(bool)), SLOT(slotConfigureDecoration()));

    m_ui->decorationList->disconnect(m_ui->decorationList->verticalScrollBar());
    m_ui->decorationList->verticalScrollBar()->disconnect(m_ui->decorationList);
    connect(m_ui->decorationList->rootObject(), SIGNAL(contentYChanged()), SLOT(updateScrollbarValue()));
    connect(m_ui->decorationList->rootObject(), SIGNAL(contentHeightChanged()), SLOT(updateScrollbarRange()));
    connect(m_ui->decorationList->verticalScrollBar(), SIGNAL(rangeChanged(int, int )), SLOT(updateScrollbarRange()));
    connect(m_ui->decorationList->verticalScrollBar(), SIGNAL(valueChanged(int)), SLOT(updateViewPosition(int)));

    m_ui->decorationList->installEventFilter(this);
    m_ui->decorationList->viewport()->installEventFilter(this);

    KAboutData *about =
        new KAboutData(I18N_NOOP("kcmkwindecoration"), 0,
                       ki18n("Window Decoration Control Module"),
                       0, KLocalizedString(), KAboutData::License_GPL,
                       ki18n("(c) 2001 Karol Szwed"));
    about->addAuthor(ki18n("Karol Szwed"), KLocalizedString(), "gallium@kde.org");
    setAboutData(about);
    QMetaObject::invokeMethod(this, "updatePreviews", Qt::QueuedConnection);
}


KWinDecorationModule::~KWinDecorationModule()
{
}

int KWinDecorationModule::itemWidth() const
{
    return m_ui->decorationList->rootObject()->property("width").toInt();
}

// This is the selection handler setting
void KWinDecorationModule::slotSelectionChanged()
{
    emit KCModule::changed(true);
}

// Reads the kwin config settings, and sets all UI controls to those settings
// Updating the config plugin if required
void KWinDecorationModule::readConfig(const KConfigGroup & conf)
{
    m_showTooltips = conf.readEntry("ShowToolTips", true);

    // Find the corresponding decoration name to that of
    // the current plugin library name

    QString libraryName = conf.readEntry("PluginLib",
                                         ((QPixmap::defaultDepth() > 8) ? "kwin3_oxygen" : "kwin3_plastik"));

    if (libraryName.isEmpty()) {
        // Selected decoration doesn't exist, use the default
        libraryName = ((QPixmap::defaultDepth() > 8) ? "kwin3_oxygen" : "kwin3_plastik");
    }

    const int bsize = conf.readEntry("BorderSize", (int)BorderNormal);
    BorderSize borderSize = BorderNormal;
    if (bsize >= BorderTiny && bsize < BordersCount)
        borderSize = static_cast< BorderSize >(bsize);
    if (libraryName == "kwin3_aurorae") {
        KConfig auroraeConfig("auroraerc");
        KConfigGroup group(&auroraeConfig, "Engine");
        const QString themeName = group.readEntry("ThemeName", "example-deco");
        const QString type = group.readEntry("EngineType", "aurorae");
        const QModelIndex index = m_proxyModel->mapFromSource(m_model->indexOfAuroraeName(themeName, type));
        if (index.isValid()) {
            m_ui->decorationList->rootObject()->setProperty("currentIndex", index.row());
        }
    } else {
        const QModelIndex index = m_proxyModel->mapFromSource(m_model->indexOfLibrary(libraryName));
        if (index.isValid()) {
            m_model->setBorderSize(index, borderSize);
            m_ui->decorationList->rootObject()->setProperty("currentIndex", index.row());
        }
    }

    // Buttons tab
    // ============
    m_decorationButtons->setCustomPositions(conf.readEntry("CustomButtonPositions", false));
    // Menu and onAllDesktops buttons are default on LHS
    m_decorationButtons->setLeftButtons(conf.readEntry("ButtonsOnLeft", KDecorationOptions::defaultTitleButtonsLeft()));
    // Help, Minimize, Maximize and Close are default on RHS
    m_decorationButtons->setRightButtons(conf.readEntry("ButtonsOnRight", KDecorationOptions::defaultTitleButtonsRight()));
    if (m_configLoaded)
        m_model->changeButtons(m_decorationButtons);
    else {
        m_configLoaded = true;
        m_model->setButtons(m_decorationButtons->customPositions(), m_decorationButtons->leftButtons(), m_decorationButtons->rightButtons());
    }

    emit KCModule::changed(false);
}


// Writes the selected user configuration to the kwin config file
void KWinDecorationModule::writeConfig(KConfigGroup & conf)
{
    const QModelIndex index = m_proxyModel->mapToSource(m_proxyModel->index(m_ui->decorationList->rootObject()->property("currentIndex").toInt(), 0));
    const QString libName = m_model->data(index, DecorationModel::LibraryNameRole).toString();

    // General settings
    conf.writeEntry("PluginLib", libName);
    conf.writeEntry("CustomButtonPositions", m_decorationButtons->customPositions());
    conf.writeEntry("ShowToolTips", m_showTooltips);

    // Button settings
    conf.writeEntry("ButtonsOnLeft", m_decorationButtons->leftButtons());
    conf.writeEntry("ButtonsOnRight", m_decorationButtons->rightButtons());
    conf.writeEntry("BorderSize",
                    static_cast<int>(m_model->data(index, DecorationModel::BorderSizeRole).toInt()));

    if (m_model->data(index, DecorationModel::TypeRole).toInt() == DecorationModelData::AuroraeDecoration ||
        m_model->data(index, DecorationModel::TypeRole).toInt() == DecorationModelData::QmlDecoration) {
        KConfig auroraeConfig("auroraerc");
        KConfigGroup group(&auroraeConfig, "Engine");
        group.writeEntry("ThemeName", m_model->data(index, DecorationModel::AuroraeNameRole).toString());
        if (m_model->data(index, DecorationModel::TypeRole).toInt() == DecorationModelData::QmlDecoration) {
            group.writeEntry("EngineType", "qml");
        } else {
            group.deleteEntry("EngineType");
        }
        group.sync();
    }

    // We saved, so tell kcmodule that there have been  no new user changes made.
    emit KCModule::changed(false);
}


// Virutal functions required by KCModule
void KWinDecorationModule::load()
{
    const KConfigGroup config(kwinConfig, "Style");

    // Reset by re-reading the config
    readConfig(config);
}


void KWinDecorationModule::save()
{
    KConfigGroup config(kwinConfig, "Style");
    writeConfig(config);
    config.sync();

    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}


void KWinDecorationModule::defaults()
{
    // Set the KDE defaults
    m_showTooltips = true;
    const QModelIndex index = m_proxyModel->mapFromSource(m_model->indexOfName(i18n("Oxygen")));
    if (index.isValid())
        m_ui->decorationList->rootObject()->setProperty("currentIndex", index.row());

    m_decorationButtons->resetToDefaults();

    m_model->changeButtons(m_decorationButtons);

    emit changed(true);
}

QString KWinDecorationModule::quickHelp() const
{
    return i18n("<h1>Window Manager Decoration</h1>"
                "<p>This module allows you to choose the window border decorations, "
                "as well as titlebar button positions and custom decoration options.</p>"
                "To choose a theme for your window decoration click on its name and apply your choice by clicking the \"Apply\" button below."
                " If you do not want to apply your choice you can click the \"Reset\" button to discard your changes."
                "<p>You can configure each theme. There are different options specific for each theme.</p>"
                "<p>On the \"Buttons\" tab check the \"Use custom titlebar button positions\" box "
                "and you can change the positions of the buttons to your liking.</p>");
}

void KWinDecorationModule::slotConfigureButtons()
{
    QPointer< KWinDecorationButtonsConfigDialog > configDialog = new KWinDecorationButtonsConfigDialog(m_decorationButtons, m_showTooltips, this);
    if (configDialog->exec() == KDialog::Accepted) {
        m_decorationButtons->setCustomPositions(configDialog->customPositions());
        m_showTooltips = configDialog->showTooltips();
        m_decorationButtons->setLeftButtons(configDialog->buttonsLeft());
        m_decorationButtons->setRightButtons(configDialog->buttonsRight());
        m_model->changeButtons(m_decorationButtons);
        emit changed(true);
    }

    delete configDialog;
}

void KWinDecorationModule::slotGHNSClicked()
{
    QPointer<KNS3::DownloadDialog> downloadDialog = new KNS3::DownloadDialog("aurorae.knsrc", this);
    if (downloadDialog->exec() == KDialog::Accepted) {
        if (!downloadDialog->changedEntries().isEmpty()) {
            const QModelIndex index = m_proxyModel->mapToSource(m_proxyModel->index(m_ui->decorationList->rootObject()->property("currentIndex").toInt(), 0));
            const QString libraryName = m_model->data(index, DecorationModel::LibraryNameRole).toString();
            bool aurorae = m_model->data(index, DecorationModel::TypeRole).toInt() == DecorationModelData::AuroraeDecoration;
            bool qml = m_model->data(index, DecorationModel::TypeRole).toInt() == DecorationModelData::QmlDecoration;
            const QString auroraeName = m_model->data(index, DecorationModel::AuroraeNameRole).toString();
            m_model->reload();
            if (aurorae) {
                const QModelIndex proxyIndex = m_proxyModel->mapFromSource(m_model->indexOfAuroraeName(auroraeName, "aurorae"));
                if (proxyIndex.isValid())
                    m_ui->decorationList->rootObject()->setProperty("currentIndex", proxyIndex.row());
            } else if (qml) {
                const QModelIndex proxyIndex = m_proxyModel->mapFromSource(m_model->indexOfAuroraeName(auroraeName, "qml"));
                if (proxyIndex.isValid())
                    m_ui->decorationList->rootObject()->setProperty("currentIndex", proxyIndex.row());
            } else {
                const QModelIndex proxyIndex = m_proxyModel->mapFromSource(m_model->indexOfLibrary(libraryName));
                if (proxyIndex.isValid())
                    m_ui->decorationList->rootObject()->setProperty("currentIndex", proxyIndex.row());
            }
        }
    }
    delete downloadDialog;
}

void KWinDecorationModule::slotConfigureDecoration()
{
    const QModelIndex index = m_proxyModel->mapToSource(m_proxyModel->index(m_ui->decorationList->rootObject()->property("currentIndex").toInt(), 0));
    bool reload = false;
    if (index.data(DecorationModel::TypeRole).toInt() == DecorationModelData::AuroraeDecoration ||
        index.data(DecorationModel::TypeRole).toInt() == DecorationModelData::QmlDecoration) {
        QPointer< KDialog > dlg = new KDialog(this);
        dlg->setCaption(i18n("Decoration Options"));
        dlg->setButtons(KDialog::Ok | KDialog::Cancel);
        KWinAuroraeConfigForm *form = new KWinAuroraeConfigForm(dlg);
        dlg->setMainWidget(form);
        form->borderSizesCombo->setCurrentIndex(index.data(DecorationModel::BorderSizeRole).toInt());
        form->buttonSizesCombo->setCurrentIndex(index.data(DecorationModel::ButtonSizeRole).toInt());
        form->closeWindowsDoubleClick->setChecked(index.data(DecorationModel::CloseOnDblClickRole).toBool());
        // in case of QmlDecoration look for a config.ui in the package structure
        KConfigDialogManager *configManager = NULL;
        if (index.data(DecorationModel::TypeRole).toInt() == DecorationModelData::QmlDecoration) {
            const QString packageName = index.data(DecorationModel::AuroraeNameRole).toString();
            const QString uiPath = KStandardDirs::locate("data", "kwin/decorations/" + packageName + "/contents/ui/config.ui");
            const QString configPath = KStandardDirs::locate("data", "kwin/decorations/" + packageName + "/contents/config/main.xml");
            if (!uiPath.isEmpty() && !configPath.isEmpty()) {
                // load the KConfigSkeleton
                QFile configFile(configPath);
                KSharedConfigPtr auroraeConfig = KSharedConfig::openConfig("auroraerc");
                KConfigGroup configGroup = auroraeConfig->group(packageName);
                Plasma::ConfigLoader *skeleton = new Plasma::ConfigLoader(&configGroup, &configFile, dlg);
                // load the ui file
                QUiLoader *loader = new QUiLoader(dlg);
                QFile uiFile(uiPath);
                uiFile.open(QFile::ReadOnly);
                QWidget *customConfigForm = loader->load(&uiFile, form);
                uiFile.close();
                form->layout()->addWidget(customConfigForm);
                // connect the ui file with the skeleton
                configManager = new KConfigDialogManager(customConfigForm, skeleton);
                configManager->updateWidgets();
            }
        }
        if (dlg->exec() == KDialog::Accepted) {
            m_model->setData(index, form->borderSizesCombo->currentIndex(), DecorationModel::BorderSizeRole);
            m_model->setData(index, form->buttonSizesCombo->currentIndex(), DecorationModel::ButtonSizeRole);
            m_model->setData(index, form->closeWindowsDoubleClick->isChecked(), DecorationModel::CloseOnDblClickRole);
            if (configManager && configManager->hasChanged()) {
                // we have a config manager and the settings changed
                configManager->updateSettings();
                m_model->notifyConfigChanged(index);
            }
            reload = true;
        }
        delete dlg;
    } else {
        QString name = index.data(DecorationModel::LibraryNameRole).toString();
        QList< QVariant > borderSizes = index.data(DecorationModel::BorderSizesRole).toList();
        const KDecorationDefines::BorderSize size =
            static_cast<KDecorationDefines::BorderSize>(index.data(DecorationModel::BorderSizeRole).toInt());
        QPointer< KWinDecorationConfigDialog > configDialog =
            new KWinDecorationConfigDialog(name, borderSizes, size, this);
        if (configDialog->exec() == KDialog::Accepted) {
            m_model->setData(index, configDialog->borderSize(), DecorationModel::BorderSizeRole);
            reload = true;
        }

        delete configDialog;
    }
    if (reload) {
        // Send signal to all kwin instances
        QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
}

bool KWinDecorationModule::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_ui->decorationList) {
        if (e->type() == QEvent::Resize)
            updateScrollbarRange();
        else if (e->type() == QEvent::KeyPress) {
            int d = 0;
            const int currentRow = m_ui->decorationList->rootObject()->property("currentIndex").toInt();
            const int key = static_cast<QKeyEvent*>(e)->key();
            switch (key) {
            case Qt::Key_Home:
                d = -currentRow;
                break;
            case Qt::Key_End:
                d = m_proxyModel->rowCount() - (1 + currentRow);
                break;
            case Qt::Key_Up:
                d = -1;
                break;
            case Qt::Key_Down:
                d = 1;
                break;
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                d = 150;
                if (QObject *decoItem = m_ui->decorationList->rootObject()->findChild<QObject*>("decorationItem")) {
                    QVariant v = decoItem->property("height");
                    if (v.isValid())
                        d = v.toInt();
                }
                if (d > 0)
                    d = qMax(m_ui->decorationList->height() / d, 1);
                if (key == Qt::Key_PageUp)
                    d = -d;
                break;
            default:
                break;
            }
            if (d) {
                d = qMin(qMax(0, currentRow + d), m_proxyModel->rowCount());
                m_ui->decorationList->rootObject()->setProperty("currentIndex", d);
                return true;
            }
        }
    } else if (m_ui->decorationList->viewport()) {
        if (e->type() == QEvent::Wheel) {
            return static_cast<QWheelEvent*>(e)->orientation() == Qt::Horizontal;
        }
    }
    return KCModule::eventFilter(o, e);
}

void KWinDecorationModule::updatePreviews()
{
    m_ui->decorationList->rootContext()->setContextProperty("sliderWidth", m_ui->decorationList->verticalScrollBar()->width());
    const int newWidth = m_ui->decorationList->rootObject()->property("width").toInt();
    if (newWidth == m_lastPreviewWidth)
        return;
    m_lastPreviewWidth = newWidth;
    const int h = m_ui->decorationList->rootObject()->property("contentHeight").toInt();
    const int y = m_ui->decorationList->rootObject()->property("contentY").toInt();
    // start at first element in sight
    int row = 0;
    if (h > 0)
        row = qMin(qMax(0, y*m_model->rowCount()/h), m_model->rowCount());
    m_model->regeneratePreviews(row);
}

void KWinDecorationModule::updatePreviewWidth()
{
    if (!m_previewUpdateTimer) {
        m_previewUpdateTimer = new QTimer(this);
        m_previewUpdateTimer->setSingleShot(true);
        connect(m_previewUpdateTimer, SIGNAL(timeout()), this, SLOT(updatePreviews()));
    }
    m_model->stopPreviewGeneration();
    m_previewUpdateTimer->start(100);
}

void KWinDecorationModule::updateScrollbarRange()
{
    m_ui->decorationList->verticalScrollBar()->blockSignals(true);
    const int h = m_ui->decorationList->rootObject()->property("contentHeight").toInt();
    m_ui->decorationList->verticalScrollBar()->setRange(0, h - m_ui->decorationList->height());
    m_ui->decorationList->verticalScrollBar()->setPageStep(m_ui->decorationList->verticalScrollBar()->maximum()/m_model->rowCount());
    m_ui->decorationList->verticalScrollBar()->blockSignals(false);
}

void KWinDecorationModule::updateScrollbarValue()
{
    const int v = m_ui->decorationList->rootObject()->property("contentY").toInt();
    m_ui->decorationList->verticalScrollBar()->blockSignals(true); // skippig this will kill kinetic scrolling but the scrollwidth is too low
    m_ui->decorationList->verticalScrollBar()->setValue(v);
    m_ui->decorationList->verticalScrollBar()->blockSignals(false);
}

void KWinDecorationModule::updateViewPosition(int v)
{
    QGraphicsObject *list = m_ui->decorationList->rootObject();
    list->setProperty("contentY", v);
}

DecorationButtons::DecorationButtons(QObject *parent)
    : QObject(parent)
    , m_customPositions(false)
    , m_leftButtons(KDecorationOptions::defaultTitleButtonsLeft())
    , m_rightButtons(KDecorationOptions::defaultTitleButtonsRight())
{
}

DecorationButtons::~DecorationButtons()
{
}

bool DecorationButtons::customPositions() const
{
    return m_customPositions;
}

const QString &DecorationButtons::leftButtons() const
{
    return m_leftButtons;
}

const QString &DecorationButtons::rightButtons() const
{
    return m_rightButtons;
}

void DecorationButtons::setCustomPositions(bool set)
{
    if (m_customPositions == set) {
        return;
    }
    m_customPositions = set;
    emit customPositionsChanged();
}

void DecorationButtons::setLeftButtons(const QString &leftButtons)
{
    if (m_leftButtons == leftButtons) {
        return;
    }
    m_leftButtons = leftButtons;
    emit leftButtonsChanged();
}

void DecorationButtons::setRightButtons (const QString &rightButtons)
{
    if (m_rightButtons == rightButtons) {
        return;
    }
    m_rightButtons = rightButtons;
    emit rightButtonsChanged();
}

void DecorationButtons::resetToDefaults()
{
    setCustomPositions(false);
    setLeftButtons(KDecorationOptions::defaultTitleButtonsLeft());
    setRightButtons(KDecorationOptions::defaultTitleButtonsRight());
}

} // namespace KWin

#include "kwindecoration.moc"
