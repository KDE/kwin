#include <QtCore/QCoreApplication>
#include <QFile>
#include <QDomDocument>
#include <QDebug>
#include <QVector>
#include <QTime>

#include <iostream>

struct Property {
    QString retType;
    QString name;
    QString type;
    QString desc;
};

struct Param {
    QString name;
    QString type;
    QString desc;
};

typedef QVector<Param> ParamList;
typedef QVector<ParamList> ParamSetList;

struct Event {
    ParamList params;
    QString wslot;
    QString wsignal;
    QString signal;
    QString desc;
    QString name;
};

struct Method {
    ParamSetList params;
    bool vparamstyle;
    bool ctor;
    QString name;
    QString retType;
    QString desc;
};

struct Class {
    QString type;
    QString name;
    QVector<Property> props;
    QVector<Method> methods;
    QVector<Event> events;
};

int main(int argc, char *argv[])
{
    QTime t;
    t.start();

    Q_UNUSED(argc)
    Q_UNUSED(argv)

    QDomDocument doc;
    QFile loc(*(argv + 1));
    QDomElement root;
    QVector<Class> classList;

    loc.open(QIODevice::ReadOnly);

    doc.setContent(&loc);
    root = doc.documentElement();

    QDomNodeList classes = root.elementsByTagName("class");

    for (unsigned int i = 0; i < classes.length(); i++) {
        Class temp;
        QDomElement classEle = classes.at(i).toElement();
        temp.name = classEle.attribute("name");
        temp.type = classEle.attribute("type");

        QDomNodeList events = classEle.elementsByTagName("event");

        for (unsigned int j = 0; j < events.length(); j++) {
            Event eTemp;
            QDomElement event = events.at(j).toElement();
            eTemp.name = event.attribute("name");
            ParamList params;

            QDomNodeList prms = event.elementsByTagName("param");

            for (unsigned int k = 0; k < prms.length(); k++) {
                Param pTemp;
                QDomElement param = prms.at(k).toElement();
                pTemp.name = param.attribute("name");
                pTemp.type = param.attribute("type");
                pTemp.desc = param.text();
                params.push_back(pTemp);
            }

            eTemp.params = params;
            eTemp.wsignal = event.attribute("wsignal", "");
            eTemp.wslot = event.attribute("wslot", "");
            eTemp.signal = event.attribute("signal", "");
            eTemp.desc = event.elementsByTagName("desc").at(0).toElement().text();
            temp.events.push_back(eTemp);
        }

        QDomNodeList methods = classEle.elementsByTagName("method");

        for (unsigned int j = 0; j < methods.length(); j++) {
            Method mTemp;
            QDomElement method = methods.at(j).toElement();
            mTemp.name = method.attribute("name");
            mTemp.ctor = false;
            ParamSetList paramsl;

            mTemp.vparamstyle = (method.attribute("vparamstyle", "") == "true");
            mTemp.ctor = (method.attribute("constructor", "") == "true") ? (true) : (false);

            if (!mTemp.vparamstyle) {
                QDomNodeList prms = method.elementsByTagName("param");
                ParamList params;

                for (unsigned int k = 0; k < prms.length(); k++) {
                    Param pTemp;
                    QDomElement param = prms.at(k).toElement();
                    pTemp.name = param.attribute("name");
                    pTemp.type = param.attribute("type");
                    pTemp.desc = param.text();
                    params.push_back(pTemp);
                }

                paramsl.push_back(params);
            } else {
                QDomNodeList pRms = method.elementsByTagName("paramset");

                for (unsigned int k = 0; k < pRms.length(); k++) {
                    QDomNodeList prms = pRms.at(k).toElement().elementsByTagName("param");
                    ParamList params;

                    for (unsigned int f = 0; f < prms.length(); f++) {
                        Param pTemp;
                        QDomElement param = prms.at(f).toElement();
                        pTemp.name = param.attribute("name");
                        pTemp.type = param.attribute("type");
                        pTemp.desc = param.text();
                        params.push_back(pTemp);
                    }

                    paramsl.push_back(params);
                }
            }

            mTemp.params = paramsl;
            mTemp.retType = method.elementsByTagName("return").at(0).toElement().attribute("type", "");
            mTemp.desc = method.elementsByTagName("desc").at(0).toElement().text();
            temp.methods.push_back(mTemp);
        }

        QDomNodeList props = classEle.elementsByTagName("property");

        for (unsigned int j = 0; j < props.length(); j++) {
            Property prTemp;
            QDomElement prop = props.at(j).toElement();
            prTemp.name = prop.attribute("name");
            prTemp.desc = prop.elementsByTagName("desc").at(0).toElement().text();
            prTemp.retType = prop.elementsByTagName("return").at(0).toElement().attribute("type", "");
            prTemp.type = prop.attribute("type");

            temp.props.push_back(prTemp);
        }

        classList.push_back(temp);
    }

    /* OUTPUT MANUAL */

    std::cout << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">" << std::endl
              << "<html xmlns=\"http://www.w3.org/1999/xhtml/\" lang=\"en\" xml:lang=\"en\">" << std::endl
              << " <head>" << std::endl
              << "  <title>KWinScripting :: APIDoX</title>" << std::endl
              << "  <link rel=\"stylesheet\" href=\"apistyle.css\" type=\"text/css\" media=\"screen\" />" << std::endl
              << " </head>" << std::endl
              << std::endl
              << " <body>" << std::endl
              << "  <div id=\"root\">" << std::endl;

    for (int i = 0; i < classList.size(); i++) {
        Class here = classList.at(i);
        std::cout << "   <div id=\"class_" << here.name.toStdString() << "\" class=\"classes\">" << std::endl
                  << "    <h2>" << here.name.toStdString() << "<sup>[<em>" << here.type.toStdString() << "</em>]</sup></h2>" << std::endl;

        if (here.events.size() != 0) {
            std::cout << "    <div id=\"events_" << here.name.toStdString() << "\" class=\"events\">" << std::endl
                      << "     <h3>Events</h3>" << std::endl;

            for (int j = 0; j < here.events.size(); j++) {
                std::cout << "     <div class=\"event\">" << std::endl
                          << "      <h4>" << here.name.toStdString() << "." << here.events.at(j).name.toStdString() << "</h4>" << std::endl
                          << "      <p class=\"desc\">" << here.events.at(j).desc.toStdString() << "</p>" << std::endl;


                ParamList p = here.events.at(j).params;

                if (p.size() > 0) {
                    std::cout << "      <ul>" << std::endl;

                    for (int k = 0; k < p.size(); k++) {
                        std::cout << "       <li><strong>" << p.at(k).name.toStdString() << " {" << p.at(k).type.toStdString() << "}</strong>: " << p.at(k).desc.toStdString() << "</li>" << std::endl;
                    }

                    std::cout << "      </ul>" << std::endl;
                }

                std::cout << "     </div>" << std::endl;
            }

            std::cout << "   </div>" << std::endl;
        }

        if (here.methods.size() != 0) {
            std::cout << "    <div id=\"method_" << here.name.toStdString() << "\" class=\"methods\">" << std::endl
                      << "     <h3>Methods</h3>" << std::endl;

            for (int j = 0; j < here.methods.size(); j++) {
                std::cout << "     <div class=\"method\">" << std::endl
                          << "      <h4>" << here.name.toStdString() << "." << here.methods.at(j).name.toStdString();

                if (here.methods.at(j).retType != "") {
                    std::cout << "[ret: " << here.methods.at(j).retType.toStdString() << "] ";
                }

                if (here.methods.at(j).ctor) {
                    std::cout << "<sup>[constructor]</sup>";
                }

                if (here.methods.at(j).vparamstyle) {
                    std::cout << "<sup>[variable parameter styles]</sup>";
                }

                std::cout << "</h4>" << std::endl;

                ParamSetList pslist = here.methods.at(j).params;

                std::cout << "      <ul>" << std::endl;

                if (pslist.size() == 0) {
                    std::cout << "       <li>" << here.name.toStdString() << "." << here.methods.at(j).name.toStdString() << "()</li>" << std::endl;
                } else {
                    for (int m = 0; m < pslist.size(); m++) {
                        ParamList plist = pslist.at(m);
                        std::cout << "       <li>" << here.name.toStdString() << "." << here.methods.at(j).name.toStdString() << "(";

                        for (int f = 0; f < plist.size(); f++) {
                            std::cout << plist.at(f).name.toStdString() << " {" << plist.at(f).type.toStdString() << "}";

                            if (f != plist.size() - 1) {
                                std::cout << ", ";
                            }
                        }

                        std::cout << ")</li>" << std::endl;
                    }
                }

                std::cout << "      </ul>" << std::endl;
                std::cout << "      <p class=\"desc\">" << here.methods.at(j).desc.toStdString() << "</p>" << std::endl;

                if (pslist.size() != 0) {
                    for (int m = 0; m < pslist.size(); m++) {
                        ParamList plist = pslist.at(m);

                        if (plist.size() == 0) {
                            continue;
                        }

                        std::cout << "       <p class=\"footdocs\">" << here.methods.at(j).name.toStdString() << "(";

                        for (int f = 0; f < plist.size(); f++) {
                            std::cout << plist.at(f).name.toStdString() << " {" << plist.at(f).type.toStdString() << "}";

                            if (f != plist.size() - 1) {
                                std::cout << ", ";
                            }
                        }

                        std::cout << ")<br />" << std::endl;
                        std::cout << "        <ul>" << std::endl;

                        for (int f = 0; f < plist.size(); f++) {
                            std::cout << "         <li><strong>" << plist.at(f).name.toStdString() << " {" << plist.at(f).type.toStdString() << "}</strong>: " << plist.at(f).desc.toStdString() << "</li>" << std::endl;
                        }

                        std::cout << "        </ul>" << std::endl;
                        std::cout << "       </p>" << std::endl;
                    }
                }

                std::cout << "     </div>" << std::endl;
            }

            std::cout << "   </div>" << std::endl;
        }

        if (here.props.size() != 0) {
            std::cout << "    <div id=\"props_" << here.name.toStdString() << "\" class=\"props\">" << std::endl
                      << "     <h3>Properties</h3>" << std::endl;

            for (int j = 0; j < here.props.size(); j++) {
                std::cout << "     <div class=\"prop\">" << std::endl
                          << "      <h4>" << here.name.toStdString() << "." << here.props.at(j).name.toStdString() << " [ret: " << here.props.at(j).retType.toStdString()
                          << "] <sup>[" << here.props.at(j).type.toStdString() << "]</sup></h4>" << std::endl
                          << "      <p class=\"desc\">" << here.props.at(j).desc.toStdString() << "</p>" << std::endl;

                std::cout << "     </div>" << std::endl;
            }

            std::cout << "   </div>" << std::endl;
        }

        std::cout << "   </div>" << std::endl;
    }

    std::cout << "  </div>" << std::endl
              << " </body>" << std::endl
              << "</html>";

    return 0;
}
