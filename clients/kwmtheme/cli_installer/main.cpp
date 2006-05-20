#include <QFile>
#include <QDir>
#include <kapplication.h>
#include <ksimpleconfig.h>
#include <kglobal.h>
#include <kdebug.h>
#include <kstandarddirs.h>
#include <kcmdlineargs.h>
#include <klocale.h>

static const char description[] =
        I18N_NOOP("Installs a KWM theme");

static KCmdLineOptions options[] =
{
    { "+[file]", I18N_NOOP("Path to a theme config file"), 0 },
    KCmdLineLastOption
};

void copy(const QString &src, const QString &dest)
{
    QFile copyInput(src);
    QFile copyOutput(dest);
    if(!copyInput.open(QIODevice::ReadOnly)){
        kWarning() << "Couldn't open " << src << endl;
        return;
    }
    if(!copyOutput.open(QIODevice::WriteOnly)){
        kWarning() << "Couldn't open " << dest << endl;
        copyInput.close();
        return;
    }
    while(!copyInput.atEnd()){
        copyOutput.putch(copyInput.getch());
    }
    copyInput.close();
    copyOutput.close();
}

int main(int argc, char **argv)
{
    KCmdLineArgs::init(argc, argv, "kwmtheme", description, "0.1");
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app(argc, argv);
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if(!args->count()){
        kWarning() << "You need to specify the path to a theme config file!" << endl;
        return(1);
    }

    QString srcStr = QString(QFile::decodeName(args->arg(0)));
    QFile f(srcStr);
    QString tmpStr;

    if(!f.exists()){
        kWarning() << "Specified theme config file doesn't exist!" << endl;
        return(2);
    }

    QStringList appDirs = KGlobal::dirs()->findDirs("data", "kwin");
    QString localDirStr = *(appDirs.end());
    if(localDirStr.isEmpty()){
        localDirStr = KGlobal::dirs()->saveLocation("data", "kwin");
    }
    localDirStr += "/pics/";
    if(!QFile::exists(localDirStr))
        QDir().mkdir(localDirStr);

    QFileInfo fi(f);
    KSimpleConfig input(fi.absoluteFilePath());
    srcStr = fi.dirPath(true) + '/';
    KConfig *output = KGlobal::config();
    input.setGroup("Window Border");
    output->setGroup("General");

    tmpStr = input.readEntry("shapePixmapTop");
    if(!tmpStr.isEmpty()){
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    }
    output->writeEntry("wm_top", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("shapePixmapBottom");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottom", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("shapePixmapLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_left", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("shapePixmapRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_right", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("shapePixmapTopLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_topleft", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("shapePixmapTopRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_topright", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("shapePixmapBottomLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottomleft", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("shapePixmapBottomRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottomright", tmpStr, KConfigBase::Normal|KConfigBase::Global);


    input.setGroup("Window Titlebar");
    output->writeEntry("TitleAlignment", input.readEntry("TitleAlignment"), KConfigBase::Normal|KConfigBase::Global);
    output->writeEntry("PixmapUnderTitleText", input.readEntry("PixmapUnderTitleText"), KConfigBase::Normal|KConfigBase::Global);
    output->writeEntry("TitleFrameShaded", input.readEntry("TitleFrameShaded"), KConfigBase::Normal|KConfigBase::Global);

    tmpStr = input.readEntry("MenuButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("menu", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("PinUpButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("pinup", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("PinDownButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("pindown", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("CloseButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("close", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("MaximizeButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("maximize", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("MaximizeDownButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("maximizedown", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("MinimizeButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("iconify", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("TitlebarPixmapActive");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("TitlebarPixmapActive", tmpStr, KConfigBase::Normal|KConfigBase::Global);
    tmpStr = input.readEntry("TitlebarPixmapInactive");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("TitlebarPixmapInactive", tmpStr, KConfigBase::Normal|KConfigBase::Global);

    input.setGroup("Window Button Layout");
    output->setGroup("Buttons");
    output->writeEntry("ButtonA", input.readEntry("ButtonA"), KConfigBase::Normal|KConfigBase::Global);
    output->writeEntry("ButtonB", input.readEntry("ButtonB"), KConfigBase::Normal|KConfigBase::Global);
    output->writeEntry("ButtonC", input.readEntry("ButtonC"), KConfigBase::Normal|KConfigBase::Global);
    output->writeEntry("ButtonD", input.readEntry("ButtonD"), KConfigBase::Normal|KConfigBase::Global);
    output->writeEntry("ButtonE", input.readEntry("ButtonE"), KConfigBase::Normal|KConfigBase::Global);
    output->writeEntry("ButtonF", input.readEntry("ButtonF"), KConfigBase::Normal|KConfigBase::Global);

    output->sync();

    return(0);
}

