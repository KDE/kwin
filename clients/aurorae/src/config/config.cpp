/********************************************************************
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "config.h"
#include "themeconfig.h"

#include <klocalizedstring.h>
#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KColorScheme>
#include <KConfig>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KGlobal>
#include <KIO/NetAccess>
#include <KMessageBox>
#include <KStandardDirs>
#include <KTar>
#include <KUrlRequesterDialog>
#include <KDE/Plasma/FrameSvg>

#include <QFile>
#include <QPainter>

extern "C"
{
    KDE_EXPORT QObject *allocate_config(KConfig *conf, QWidget *parent)
    {
        return (new Aurorae::AuroraeConfig(conf, parent));
    }
}

namespace Aurorae
{

//Theme selector code by Andre Duffeck (modified to add package description)
ThemeModel::ThemeModel(QObject *parent)
        : QAbstractListModel(parent)
{
    reload();
}

ThemeModel::~ThemeModel()
{
    clearThemeList();
}

void ThemeModel::clearThemeList()
{
    foreach(const ThemeInfo &themeInfo, m_themes) {
        delete themeInfo.svg;
    }
    m_themes.clear();
}

void ThemeModel::reload()
{
    reset();
    clearThemeList();

    // get all desktop themes
    QStringList themes = KGlobal::dirs()->findAllResources("data",
                                                            "aurorae/themes/*/metadata.desktop",
                                                            KStandardDirs::NoDuplicates);
    foreach(const QString &theme, themes) {
        int themeSepIndex = theme.lastIndexOf('/', -1);
        QString themeRoot = theme.left(themeSepIndex);
        int themeNameSepIndex = themeRoot.lastIndexOf('/', -1);
        QString packageName = themeRoot.right(themeRoot.length() - themeNameSepIndex - 1);

        KDesktopFile df(theme);
        QString name = df.readName();
        if (name.isEmpty()) {
            name = packageName;
        }
        QString comment = df.readComment();
        QString author = df.desktopGroup().readEntry("X-KDE-PluginInfo-Author", QString());
        QString email = df.desktopGroup().readEntry("X-KDE-PluginInfo-Email", QString());
        QString version = df.desktopGroup().readEntry("X-KDE-PluginInfo-Version", QString());
        QString license = df.desktopGroup().readEntry("X-KDE-PluginInfo-License", QString());
        QString website = df.desktopGroup().readEntry("X-KDE-PluginInfo-Website", QString());


        Plasma::FrameSvg *svg = new Plasma::FrameSvg(this);
        QString svgFile = themeRoot + "/decoration.svg";
        if (QFile::exists(svgFile)) {
            svg->setImagePath(svgFile);
        } else {
            svg->setImagePath(svgFile + 'z');
        }
        svg->setEnabledBorders(Plasma::FrameSvg::AllBorders);

        ThemeConfig *config = new ThemeConfig();
        KConfig conf("aurorae/themes/" + packageName + '/' + packageName + "rc", KConfig::FullConfig, "data");
        config->load(&conf);

        // buttons
        QHash<QString, Plasma::FrameSvg*> *buttons = new QHash<QString, Plasma::FrameSvg*>();
        initButtonFrame("minimize", packageName, buttons);
        initButtonFrame("maximize", packageName, buttons);
        initButtonFrame("restore", packageName, buttons);
        initButtonFrame("close", packageName, buttons);
        initButtonFrame("alldesktops", packageName, buttons);
        initButtonFrame("keepabove", packageName, buttons);
        initButtonFrame("keepbelow", packageName, buttons);
        initButtonFrame("shade", packageName, buttons);
        initButtonFrame("help", packageName, buttons);

        ThemeInfo info;
        info.package = packageName;
        info.description = comment;
        info.author = author;
        info.email = email;
        info.version = version;
        info.website = website;
        info.license = license;
        info.svg = svg;
        info.themeRoot = themeRoot;
        info.themeConfig = config;
        info.buttons = buttons;
        m_themes[name] = info;
    }

    beginInsertRows(QModelIndex(), 0, m_themes.size());
    endInsertRows();
}

int ThemeModel::rowCount(const QModelIndex &) const
{
    return m_themes.size();
}

QVariant ThemeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= m_themes.size()) {
        return QVariant();
    }

    QMap<QString, ThemeInfo>::const_iterator it = m_themes.constBegin();
    for (int i = 0; i < index.row(); ++i) {
        ++it;
    }

    switch (role) {
    case Qt::DisplayRole:
        return it.key();
    case PackageNameRole:
        return (*it).package;
    case SvgRole:
        return qVariantFromValue((void*)(*it).svg);
    case PackageDescriptionRole:
        return (*it).description;
    case PackageAuthorRole:
        return (*it).author;
    case PackageVersionRole:
        return (*it).version;
    case PackageEmailRole:
        return (*it).email;
    case PackageLicenseRole:
        return (*it).license;
    case PackageWebsiteRole:
        return (*it).website;
    case ThemeConfigRole:
        return qVariantFromValue((void*)(*it).themeConfig);
    case ButtonsRole:
        return qVariantFromValue((void*)(*it).buttons);
    default:
        return QVariant();
    }
}

int ThemeModel::indexOf(const QString &name) const
{
    QMapIterator<QString, ThemeInfo> it(m_themes);
    int i = -1;
    while (it.hasNext()) {
        ++i;
        if (it.next().value().package == name) {
            return i;
        }
    }

    return -1;
}

void ThemeModel::initButtonFrame(const QString &button, const QString &themeName, QHash<QString, Plasma::FrameSvg*> *buttons)
{
    QString file("aurorae/themes/" + themeName + '/' + button + ".svg");
    QString path = KGlobal::dirs()->findResource("data", file);
    if (path.isEmpty()) {
        // let's look for svgz
        file.append("z");
        path = KGlobal::dirs()->findResource("data", file);
    }
    if (!path.isEmpty()) {
        Plasma::FrameSvg *frame = new Plasma::FrameSvg(this);
        frame->setImagePath(path);
        frame->setCacheAllRenderedFrames(true);
        frame->setEnabledBorders(Plasma::FrameSvg::NoBorder);
        buttons->insert(button, frame);
    }
}

///////////////////////////////////////////////////
// ThemeDelegate
//////////////////////////////////////////////////
ThemeDelegate::ThemeDelegate(QObject* parent)
        : QAbstractItemDelegate(parent)
{
}

void ThemeDelegate::paint(QPainter *painter,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
{
    QString title = index.model()->data(index, Qt::DisplayRole).toString();

    // highlight selected item
    painter->save();
    if (option.state & QStyle::State_Selected) {
        painter->setBrush(option.palette.color(QPalette::Highlight));
    } else {
        painter->setBrush(Qt::gray);
    }
    painter->drawRect(option.rect);
    painter->restore();

    ThemeConfig *themeConfig = static_cast<ThemeConfig *>(
    index.model()->data(index, ThemeModel::ThemeConfigRole).value<void *>());
    painter->save();
    paintDeco(painter, false, option, index, 5 + themeConfig->paddingLeft() + themeConfig->borderLeft(),
               5, 5, 5 + themeConfig->paddingBottom() + themeConfig->borderBottom() );
    painter->restore();
    painter->save();
    int activeLeft = 5;
    int activeTop = 5 + themeConfig->paddingTop() + themeConfig->titleEdgeTop() +
        themeConfig->titleEdgeBottom() + themeConfig->titleHeight();
    int activeRight = 5 + themeConfig->paddingRight() + themeConfig->borderRight();
    int activeBottom = 5;
    paintDeco(painter, true,  option, index, activeLeft, activeTop, activeRight, activeBottom);
    painter->restore();

    // paint title
    painter->save();
    QFont font = painter->font();
    font.setWeight(QFont::Bold);
    painter->setPen(themeConfig->activeTextColor());
    painter->setFont(font);
    painter->drawText(QRect(option.rect.topLeft() + QPoint(activeLeft, activeTop),
                             option.rect.bottomRight() - QPoint(activeRight, activeBottom)),
                       Qt::AlignCenter | Qt::TextWordWrap, title);
    painter->restore();
}

void ThemeDelegate::paintDeco(QPainter *painter, bool active, const QStyleOptionViewItem &option, const QModelIndex &index,
                               int leftMargin, int topMargin,
                               int rightMargin, int bottomMargin) const
{
    Plasma::FrameSvg *svg = static_cast<Plasma::FrameSvg *>(
                                index.model()->data(index, ThemeModel::SvgRole).value<void *>());
    svg->setElementPrefix("decoration");
    if (!active && svg->hasElementPrefix("decoration-inactive")) {
        svg->setElementPrefix("decoration-inactive");
    }
    svg->resizeFrame(QSize(option.rect.width() - leftMargin - rightMargin, option.rect.height() - topMargin - bottomMargin));
    svg->paintFrame(painter, option.rect.topLeft() + QPoint(leftMargin, topMargin));

    ThemeConfig *themeConfig = static_cast<ThemeConfig *>(
                                    index.model()->data(index, ThemeModel::ThemeConfigRole).value<void *>());

    QHash<QString, Plasma::FrameSvg*> *buttons = static_cast<QHash<QString, Plasma::FrameSvg*> *>(
                                                    index.model()->data(index, ThemeModel::ButtonsRole).value<void *>());
    int y = option.rect.top() + topMargin + themeConfig->paddingTop() + themeConfig->titleEdgeTop() + themeConfig->buttonMarginTop();
    int x = option.rect.left() + leftMargin + themeConfig->paddingLeft() + themeConfig->titleEdgeLeft();
    int buttonWidth = themeConfig->buttonWidth();
    int buttonHeight = themeConfig->buttonHeight();
    foreach (const QChar &character, themeConfig->defaultButtonsLeft()) {
        QString buttonName;
        int width = buttonWidth;
        if (character == '_'){
            x += themeConfig->explicitButtonSpacer() + themeConfig->buttonSpacing();
            continue;
        }
        else if (character == 'M') {
            KIcon icon = KIcon( "xorg" );
            QSize buttonSize(buttonWidth,buttonHeight);
            painter->drawPixmap(QPoint(x,y), icon.pixmap(buttonSize));
            x += themeConfig->buttonWidthMenu();
        }
        else if (character == 'S') {
            buttonName = "alldesktops";
            width = themeConfig->buttonWidthAllDesktops();
        }
        else if (character == 'H') {
            buttonName = "help";
            width = themeConfig->buttonWidthHelp();
        }
        else if (character == 'I') {
            buttonName = "minimize";
            width = themeConfig->buttonWidthMinimize();
        }
        else if (character == 'A') {
            buttonName = "restore";
            if (!buttons->contains(buttonName)) {
                buttonName = "maximize";
            }
            width = themeConfig->buttonWidthMaximizeRestore();
        }
        else if (character == 'X') {
            buttonName = "close";
            width = themeConfig->buttonWidthClose();
        }
        else if (character == 'F') {
            buttonName = "keepabove";
            width = themeConfig->buttonWidthKeepAbove();
        }
        else if (character == 'B') {
            buttonName = "keepbelow";
            width = themeConfig->buttonWidthKeepBelow();
        }
        else if (character == 'L') {
            buttonName = "shade";
            width = themeConfig->buttonWidthShade();
        }
        if (!buttonName.isEmpty() && buttons->contains(buttonName)) {
            Plasma::FrameSvg *frame = buttons->value(buttonName);
            frame->setElementPrefix("active");
            if (!active && frame->hasElementPrefix("inactive")) {
                frame->setElementPrefix("inactive");
            }
            frame->resizeFrame(QSize(width,buttonHeight));
            frame->paintFrame(painter, QPoint(x, y));
            x += width;
        }
        x += themeConfig->buttonSpacing();
    }
    if (!themeConfig->defaultButtonsLeft().isEmpty()) {
        x -= themeConfig->buttonSpacing();
    }
    int titleLeft = x;

    x = option.rect.right() - rightMargin - themeConfig->paddingRight() - themeConfig->titleEdgeRight();
    QString rightButtons;
    foreach (const QChar &character, themeConfig->defaultButtonsRight()) {
        rightButtons.prepend(character);
    }
    foreach (const QChar &character, rightButtons) {
        QString buttonName;
        int width = buttonWidth;
        if (character == '_'){
            x -= themeConfig->explicitButtonSpacer() + themeConfig->buttonSpacing();
            continue;
        }
        else if (character == 'M') {
            KIcon icon = KIcon( "xorg" );
            QSize buttonSize(buttonWidth,buttonHeight);
            x -= themeConfig->buttonWidthMenu();
            painter->drawPixmap(QPoint(x,y), icon.pixmap(buttonSize));
        }
        else if (character == 'S') {
            buttonName = "alldesktops";
            width = themeConfig->buttonWidthAllDesktops();
        }
        else if (character == 'H') {
            buttonName = "help";
            width = themeConfig->buttonWidthHelp();
        }
        else if (character == 'I') {
            buttonName = "minimize";
            width = themeConfig->buttonWidthMinimize();
        }
        else if (character == 'A') {
            buttonName = "restore";
            if (!buttons->contains(buttonName)) {
                buttonName = "maximize";
            }
            width = themeConfig->buttonWidthMaximizeRestore();
        }
        else if (character == 'X') {
            buttonName = "close";
            width = themeConfig->buttonWidthClose();
        }
        else if (character == 'F') {
            buttonName = "keepabove";
            width = themeConfig->buttonWidthKeepAbove();
        }
        else if (character == 'B') {
            buttonName = "keepbelow";
            width = themeConfig->buttonWidthKeepBelow();
        }
        else if (character == 'L') {
            buttonName = "shade";
            width = themeConfig->buttonWidthShade();
        }
        if (!buttonName.isEmpty() && buttons->contains(buttonName)) {
            Plasma::FrameSvg *frame = buttons->value(buttonName);
            frame->setElementPrefix("active");
            if (!active && frame->hasElementPrefix("inactive")) {
                frame->setElementPrefix("inactive");
            }
            frame->resizeFrame(QSize(width,buttonHeight));
            x -= width;
            frame->paintFrame(painter, QPoint(x, y));
        }
        x -= themeConfig->buttonSpacing();
    }
    if (!rightButtons.isEmpty()){
        x += themeConfig->buttonSpacing();
    }
    int titleRight = x;

    // draw text
    painter->save();
    if (active) {
        painter->setPen(themeConfig->activeTextColor());
    }
    else {
        painter->setPen(themeConfig->inactiveTextColor());
    }
    y = option.rect.top() + topMargin + themeConfig->paddingTop() + themeConfig->titleEdgeTop();
    QRectF titleRect(QPointF(titleLeft, y), QPointF(titleRight, y + themeConfig->titleHeight()));
    QString caption = i18n("Active Window");
    if (!active) {
        caption = i18n("Inactive Window");
    }
    painter->drawText(titleRect,
                       themeConfig->alignment() | themeConfig->verticalAlignment() | Qt::TextSingleLine,
                       caption);
    painter->restore();
}

QSize ThemeDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return QSize(400, 200);
}

///////////////////////////////////////////////////
// AuroraeConfig
//////////////////////////////////////////////////

AuroraeConfig::AuroraeConfig(KConfig* conf, QWidget* parent)
        : QObject(parent)
        , m_parent(parent)
{
    Q_UNUSED(conf)
    m_ui = new AuroraeConfigUI(parent);
    m_ui->aboutPushButton->setIcon(KIcon("dialog-information"));

    m_themeModel = new ThemeModel(this);
    m_ui->theme->setModel(m_themeModel);
    m_ui->theme->setItemDelegate(new ThemeDelegate(m_ui->theme->view()));
    m_ui->theme->setMinimumSize(400, m_ui->theme->sizeHint().height());

    m_config = new KConfig("auroraerc");
    KConfigGroup group(m_config, "Engine");
    load(group);

    connect(m_ui->theme, SIGNAL(currentIndexChanged(int)), this, SIGNAL(changed()));
    connect(m_ui->installNewThemeButton, SIGNAL(clicked(bool)), this, SLOT(slotInstallNewTheme()));
    connect(m_ui->aboutPushButton, SIGNAL(clicked(bool)), this, SLOT(slotAboutClicked()));
    m_ui->show();
}

AuroraeConfig::AuroraeConfig::~AuroraeConfig()
{
    delete m_ui;
    delete m_config;
}

void AuroraeConfig::defaults()
{
    m_ui->theme->setCurrentIndex(m_themeModel->indexOf("example-deco"));
}

void AuroraeConfig::load(const KConfigGroup &conf)
{
    QString theme = conf.readEntry("ThemeName", "example-deco");
    m_ui->theme->setCurrentIndex(m_themeModel->indexOf(theme));
}

void AuroraeConfig::save(KConfigGroup &conf)
{
    Q_UNUSED(conf)
    KConfigGroup group(m_config, "Engine");
    int index = m_ui->theme->currentIndex();
    QString theme = m_ui->theme->itemData(index, ThemeModel::PackageNameRole).toString();
    group.writeEntry("ThemeName", theme);
    group.sync();
}

void AuroraeConfig::slotAboutClicked()
{
    int index = m_ui->theme->currentIndex();
    const QString name = m_ui->theme->itemData(index, Qt::DisplayRole).toString();
    const QString comment = m_ui->theme->itemData(index, ThemeModel::PackageDescriptionRole).toString();
    const QString author = m_ui->theme->itemData(index, ThemeModel::PackageAuthorRole).toString();
    const QString email = m_ui->theme->itemData(index, ThemeModel::PackageEmailRole).toString();
    const QString website = m_ui->theme->itemData(index, ThemeModel::PackageWebsiteRole).toString();
    const QString version = m_ui->theme->itemData(index, ThemeModel::PackageVersionRole).toString();
    const QString license = m_ui->theme->itemData(index, ThemeModel::PackageLicenseRole).toString();

    KAboutData aboutData(name.toUtf8(), name.toUtf8(), ki18n(name.toUtf8()), version.toUtf8(), ki18n(comment.toUtf8()), KAboutLicense::byKeyword(license).key(), ki18n(QByteArray()), ki18n(QByteArray()), website.toLatin1());
    aboutData.setProgramIconName("preferences-system-windows-action");
    const QStringList authors = author.split(',');
    const QStringList emails = email.split(',');
    int i = 0;
    if (authors.count() == emails.count()) {
        foreach(const QString &author, authors) {
            if (!author.isEmpty()) {
                aboutData.addAuthor(ki18n(author.toUtf8()), ki18n(QByteArray()), emails[i].toUtf8(), 0);
            }
            i++;
        }
    }
    KAboutApplicationDialog aboutPlugin(&aboutData, m_parent);
    aboutPlugin.exec();
}

void AuroraeConfig::slotInstallNewTheme()
{
    KUrl themeURL = KUrlRequesterDialog::getUrl(QString(), m_parent,
                    i18n("Drag or Type Theme URL"));
    if (themeURL.url().isEmpty()) {
        return;
    }

    // themeTmpFile contains the name of the downloaded file
    QString themeTmpFile;

    if (!KIO::NetAccess::download(themeURL, themeTmpFile, m_parent)) {
        QString sorryText;
        if (themeURL.isLocalFile()) {
            sorryText = i18n("Unable to find the theme archive %1.", themeURL.prettyUrl());
        }
        else {
            sorryText = i18n("Unable to download theme archive;\n"
                             "please check that address %1 is correct.", themeURL.prettyUrl());
        }
        KMessageBox::sorry(m_parent, sorryText);
        return ;
    }

    // TODO: check if archive contains a valid theme
    // TODO: show a progress dialog
    KTar archive(themeTmpFile);
    archive.open(QIODevice::ReadOnly);
    const KArchiveDirectory* themeDir = archive.directory();
    QString localThemesDir = KStandardDirs::locateLocal("data", "aurorae/themes/");
    foreach(const QString& entry, themeDir->entries()) {
        // entry has to be a directory to contain a theme
        const KArchiveEntry* possibleEntry = themeDir->entry(entry);
        if (possibleEntry->isDirectory()) {
            const KArchiveDirectory* dir = dynamic_cast<const KArchiveDirectory*>(possibleEntry);
            if (dir) {
                dir->copyTo(localThemesDir + dir->name());
            }
        }
    }
    // and reload
    int index = m_ui->theme->currentIndex();
    const QString themeName = m_ui->theme->itemData(index, ThemeModel::PackageNameRole).toString();
    m_themeModel->reload();
    m_ui->theme->setCurrentIndex(m_themeModel->indexOf(themeName));
}

} // namespace

#include "config.moc"
