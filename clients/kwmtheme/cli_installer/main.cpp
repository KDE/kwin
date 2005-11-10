#include <qfile.h>
#include <qdir.h>
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
        kdWarning() << "Couldn't open " << src << endl;
        return;
    }
    if(!copyOutput.open(QIODevice::WriteOnly)){
        kdWarning() << "Couldn't open " << dest << endl;
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
        kdWarning() << "You need to specify the path to a theme config file!" << endl;
        return(1);
    }

    QString srcStr = QString(QFile::decodeName(args->arg(0)));
    QFile f(srcStr);
    QString tmpStr;

    if(!f.exists()){
        kdWarning() << "Specified theme config file doesn't exist!" << endl;
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
    srcStr = fi.dirPath(true) + "/";
    KConfig *output = KGlobal::config();
    input.setGroup("Window Border");
    output->setGroup("General");

    tmpStr = input.readEntry("shapePixmapTop");
    if(!tmpStr.isEmpty()){
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    }
    output->writeEntry("wm_top", tmpStr, true, true);
    tmpStr = input.readEntry("shapePixmapBottom");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottom", tmpStr, true, true);
    tmpStr = input.readEntry("shapePixmapLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_left", tmpStr, true, true);
    tmpStr = input.readEntry("shapePixmapRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_right", tmpStr, true, true);
    tmpStr = input.readEntry("shapePixmapTopLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_topleft", tmpStr, true, true);
    tmpStr = input.readEntry("shapePixmapTopRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_topright", tmpStr, true, true);
    tmpStr = input.readEntry("shapePixmapBottomLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottomleft", tmpStr, true, true);
    tmpStr = input.readEntry("shapePixmapBottomRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottomright", tmpStr, true, true);


    input.setGroup("Window Titlebar");
    output->writeEntry("TitleAlignment", input.readEntry("TitleAlignment"), true, true);
    output->writeEntry("PixmapUnderTitleText", input.readEntry("PixmapUnderTitleText"), true, true);
    output->writeEntry("TitleFrameShaded", input.readEntry("TitleFrameShaded"), true, true);

    tmpStr = input.readEntry("MenuButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("menu", tmpStr, true, true);
    tmpStr = input.readEntry("PinUpButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("pinup", tmpStr, true, true);
    tmpStr = input.readEntry("PinDownButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("pindown", tmpStr, true, true);
    tmpStr = input.readEntry("CloseButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("close", tmpStr, true, true);
    tmpStr = input.readEntry("MaximizeButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("maximize", tmpStr, true, true);
    tmpStr = input.readEntry("MaximizeDownButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("maximizedown", tmpStr, true, true);
    tmpStr = input.readEntry("MinimizeButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("iconify", tmpStr, true, true);
    tmpStr = input.readEntry("TitlebarPixmapActive");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("TitlebarPixmapActive", tmpStr, true, true);
    tmpStr = input.readEntry("TitlebarPixmapInactive");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("TitlebarPixmapInactive", tmpStr, true, true);

    input.setGroup("Window Button Layout");
    output->setGroup("Buttons");
    output->writeEntry("ButtonA", input.readEntry("ButtonA"), true, true);
    output->writeEntry("ButtonB", input.readEntry("ButtonB"), true, true);
    output->writeEntry("ButtonC", input.readEntry("ButtonC"), true, true);
    output->writeEntry("ButtonD", input.readEntry("ButtonD"), true, true);
    output->writeEntry("ButtonE", input.readEntry("ButtonE"), true, true);
    output->writeEntry("ButtonF", input.readEntry("ButtonF"), true, true);

    output->sync();

    return(0);
}

