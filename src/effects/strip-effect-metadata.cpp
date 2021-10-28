/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>

/**
 * This little helper strips unnecessary information from builtin effect metadata files to
 * reduce the size of kwin executables and json parsing runtime overhead.
 */

static bool stripMetaData(const QString &fileName, const QString &target)
{
    QFile originalFile(fileName);
    if (!originalFile.open(QFile::ReadOnly)) {
        qWarning("Failed to open %s: %s", qPrintable(fileName), qPrintable(originalFile.errorString()));
        return false;
    }

    QJsonDocument fullDocument = QJsonDocument::fromJson(originalFile.readAll());
    if (fullDocument.isNull()) {
        qWarning() << "Invalid metadata in" << fileName;
        return false;
    }

    const QJsonObject originalRootObject = fullDocument.object();

    QJsonObject kpluginObject;
    kpluginObject["Id"] = originalRootObject["KPlugin"]["Id"];
    kpluginObject["EnabledByDefault"] = originalRootObject["KPlugin"]["EnabledByDefault"];

    QJsonObject strippedRootObject;
    strippedRootObject["KPlugin"] = kpluginObject;

    QFile targetFile(target);
    if (!targetFile.open(QFile::WriteOnly)) {
        qWarning("Failed to open %s: %s", qPrintable(target), qPrintable(targetFile.errorString()));
        return false;
    }

    targetFile.write(QJsonDocument(strippedRootObject).toJson());
    targetFile.close();

    return targetFile.error() == QFile::NoError;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("kwin-strip-effect-metadata");

    QCommandLineOption inputOption(QStringLiteral("source"), QStringLiteral("input file"), QStringLiteral("path/to/file"));
    parser.addOption(inputOption);

    QCommandLineOption outputOption(QStringLiteral("output"), QStringLiteral("output file"), QStringLiteral("path/to/file"));
    parser.addOption(outputOption);

    parser.process(app);
    return !stripMetaData(parser.value(inputOption), parser.value(outputOption));
}
