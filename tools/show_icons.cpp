#include <kcmdlineargs.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kaboutdata.h>
#include <kwindowsystem.h>
#include <qlabel.h>
#include <qgridlayout.h>
#include <netwm.h>

int main(int argc, char* argv[])
{
    KAboutData about("a", "b", ki18n("c"), "d");
    KCmdLineArgs::init(argc, argv, &about);
    KCmdLineOptions args;
    args.add("window <n>", ki18n("Window to show icons for"));
    KCmdLineArgs::addCmdLineOptions(args);
    KApplication app;
    QWidget w;
    QGridLayout l(&w);
    l.setSpacing(5);
    WId window = KCmdLineArgs::parsedArgs()->getOption("window").toLong();
    NETWinInfo info(QX11Info::display(), window, QX11Info::appRootWindow(), NET::WMIcon);
    const int* sizes = info.iconSizes();
    int i = 0;
    for (;
            sizes[ i * 2 ] != 0;
            ++i) {
        int width = sizes[ i * 2 ];
        int height = sizes[ i * 2 + 1 ];
        l.addWidget(new QLabel(QString("EWMH: %1x%2").arg(width).arg(height), &w), 0, i);
        QLabel* ll = new QLabel(&w);
        ll->setPixmap(KWindowSystem::icon(window, width, height, KWindowSystem::NETWM));
        l.addWidget(ll, 1, i, Qt::AlignCenter);
    }
    QLabel* ll;
    l.addWidget(new QLabel("ICCCM", &w), 0, i);
    ll = new QLabel(&w);
    ll->setPixmap(KWindowSystem::icon(window, -1, -1, KWindowSystem::WMHints));
    l.addWidget(ll, 1, i, Qt::AlignCenter);
    ++i;
    l.addWidget(new QLabel("CLASS", &w), 0, i);
    ll = new QLabel(&w);
    ll->setPixmap(KWindowSystem::icon(window, -1, -1, KWindowSystem::WMHints));
    l.addWidget(ll, 1, i, Qt::AlignCenter);
    w.show();
    return app.exec();
}
