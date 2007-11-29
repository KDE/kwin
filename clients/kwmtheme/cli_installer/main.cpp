/********************************************************************
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

void copy(const QString &src, const QString &dest)
{
    QFile copyInput(src);
    QFile copyOutput(dest);
    if(!copyInput.open(QIODevice::ReadOnly)){
        kWarning() << "Couldn't open " << src ;
        return;
    }
    if(!copyOutput.open(QIODevice::WriteOnly)){
        kWarning() << "Couldn't open " << dest ;
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

    KCmdLineOptions options;
    options.add("+[file]", ki18n("Path to a theme config file"));
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app(argc, argv);
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if(!args->count()){
        kWarning() << "You need to specify the path to a theme config file!" ;
        return(1);
    }

    QString srcStr = QString(args->arg(0));
    QFile f(srcStr);
    QString tmpStr;

    if(!f.exists()){
        kWarning() << "Specified theme config file doesn't exist!" ;
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
    KSharedConfig::Ptr output = KGlobal::config();
    input.setGroup("Window Border");
    output->setGroup("General");

    tmpStr = input.readEntry("shapePixmapTop");
    if(!tmpStr.isEmpty()){
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    }
    output->writeEntry("wm_top", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("shapePixmapBottom");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottom", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("shapePixmapLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_left", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("shapePixmapRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_right", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("shapePixmapTopLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_topleft", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("shapePixmapTopRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_topright", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("shapePixmapBottomLeft");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottomleft", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("shapePixmapBottomRight");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("wm_bottomright", tmpStr, KConfig::Normal|KConfig::Global);


    input.setGroup("Window Titlebar");
    output->writeEntry("TitleAlignment", input.readEntry("TitleAlignment"), KConfig::Normal|KConfig::Global);
    output->writeEntry("PixmapUnderTitleText", input.readEntry("PixmapUnderTitleText"), KConfig::Normal|KConfig::Global);
    output->writeEntry("TitleFrameShaded", input.readEntry("TitleFrameShaded"), KConfig::Normal|KConfig::Global);

    tmpStr = input.readEntry("MenuButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("menu", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("PinUpButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("pinup", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("PinDownButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("pindown", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("CloseButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("close", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("MaximizeButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("maximize", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("MaximizeDownButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("maximizedown", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("MinimizeButton");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("iconify", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("TitlebarPixmapActive");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("TitlebarPixmapActive", tmpStr, KConfig::Normal|KConfig::Global);
    tmpStr = input.readEntry("TitlebarPixmapInactive");
    if(!tmpStr.isEmpty())
        copy(srcStr+tmpStr, localDirStr+tmpStr);
    output->writeEntry("TitlebarPixmapInactive", tmpStr, KConfig::Normal|KConfig::Global);

    input.setGroup("Window Button Layout");
    output->setGroup("Buttons");
    output->writeEntry("ButtonA", input.readEntry("ButtonA"), KConfig::Normal|KConfig::Global);
    output->writeEntry("ButtonB", input.readEntry("ButtonB"), KConfig::Normal|KConfig::Global);
    output->writeEntry("ButtonC", input.readEntry("ButtonC"), KConfig::Normal|KConfig::Global);
    output->writeEntry("ButtonD", input.readEntry("ButtonD"), KConfig::Normal|KConfig::Global);
    output->writeEntry("ButtonE", input.readEntry("ButtonE"), KConfig::Normal|KConfig::Global);
    output->writeEntry("ButtonF", input.readEntry("ButtonF"), KConfig::Normal|KConfig::Global);

    output->sync();

    return(0);
}

