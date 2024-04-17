/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
** Contact: lorn.potter@gmail.com
**
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
****************************************************************************/

#include "declarativeconnectionagent.h"
#include "connectiond_interface.h"

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"

DeclarativeConnectionAgent::DeclarativeConnectionAgent(QObject *parent)
    : QObject(parent),
      connManagerInterface(nullptr)
{
    connectiondWatcher = new QDBusServiceWatcher(CONND_SERVICE, QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForRegistration
            | QDBusServiceWatcher::WatchForUnregistration, this);

    connect(connectiondWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, &DeclarativeConnectionAgent::connectToConnectiond);
    connect(connectiondWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &DeclarativeConnectionAgent::connectiondUnregistered);

    connectToConnectiond();
}

DeclarativeConnectionAgent::~DeclarativeConnectionAgent()
{
}

void DeclarativeConnectionAgent::connectToConnectiond()
{
    delete connManagerInterface;
    connManagerInterface = nullptr;

    if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(CONND_SERVICE)) {
        qDebug() << Q_FUNC_INFO << QString("connection service not available").arg(CONND_SERVICE);
        QDBusReply<void> reply = QDBusConnection::sessionBus().interface()->startService(CONND_SERVICE);

        if (!reply.isValid()) {
            qDebug() << Q_FUNC_INFO << reply.error().message();
            return;
        }
    }

    connManagerInterface = new com::jolla::Connectiond(CONND_SERVICE, CONND_PATH,
                                                       QDBusConnection::sessionBus(), this);
    if (!connManagerInterface->isValid()) {
        qDebug() << Q_FUNC_INFO << "is not valid interface";
    }
    connect(connManagerInterface, &com::jolla::Connectiond::connectionRequest,
            this, &DeclarativeConnectionAgent::connectionRequest);
    connect(connManagerInterface, &com::jolla::Connectiond::configurationNeeded,
            this, &DeclarativeConnectionAgent::configurationNeeded);
    connect(connManagerInterface, &com::jolla::Connectiond::userInputCanceled,
            this, &DeclarativeConnectionAgent::userInputCanceled);
    connect(connManagerInterface, &com::jolla::Connectiond::errorReported,
            this, &DeclarativeConnectionAgent::errorReported);
    connect(connManagerInterface, &com::jolla::Connectiond::connectionState,
            this, &DeclarativeConnectionAgent::connectionState);
    connect(connManagerInterface, &com::jolla::Connectiond::requestBrowser,
            this, &DeclarativeConnectionAgent::browserRequested);
    connect(connManagerInterface, &com::jolla::Connectiond::userInputRequested,
            this, &DeclarativeConnectionAgent::onUserInputRequested, Qt::UniqueConnection);
    connect(connManagerInterface, &com::jolla::Connectiond::bluetoothTetheringFinished,
            this, &DeclarativeConnectionAgent::bluetoothTetheringFinished);
    connect(connManagerInterface, &com::jolla::Connectiond::wifiTetheringFinished,
            this, &DeclarativeConnectionAgent::wifiTetheringFinished);
}

void DeclarativeConnectionAgent::sendUserReply(const QVariantMap &input)
{
    if (!checkValidness()) {
        return;
    }

    QDBusPendingReply<> reply = connManagerInterface->sendUserReply(input);
    reply.waitForFinished();
    if (reply.isError()) {
        qDebug() << Q_FUNC_INFO << reply.error().message();
        Q_EMIT errorReported("", reply.error().message());
    }
}

void DeclarativeConnectionAgent::sendConnectReply(const QString &replyMessage, int timeout)
{
    if (!checkValidness()) {
        return;
    }

    connManagerInterface->sendConnectReply(replyMessage,timeout);
}

void DeclarativeConnectionAgent::connectToType(const QString &type)
{
    if (!checkValidness()) {
        return;
    }

    connManagerInterface->connectToType(type);
}

void DeclarativeConnectionAgent::onUserInputRequested(const QString &service, const QVariantMap &fields)
{
    // we do this as qtdbus does not understand QVariantMap very well.
    // we need to manually demarshall
    QVariantMap map;
    QMapIterator<QString, QVariant> i(fields);
    // this works for Passphrase at least. anything else?
    while (i.hasNext()) {
        i.next();
        QDBusArgument arg = fields.value(i.key()).value<QDBusArgument>();
        QVariantMap vmap = qdbus_cast<QVariantMap>(arg);
        map.insert(i.key(), vmap);
    }
    Q_EMIT userInputRequested(service, map);
}

void DeclarativeConnectionAgent::connectiondUnregistered()
{
    delete connManagerInterface;
    connManagerInterface = nullptr;
}

void DeclarativeConnectionAgent::startTethering(const QString &type)
{
    if (!checkValidness()) {
        return;
    }

    connManagerInterface->startTethering(type);
}

void DeclarativeConnectionAgent::stopTethering(const QString &type, bool keepPowered)
{
    if (!checkValidness()) {
        return;
    }

    connManagerInterface->stopTethering(type, keepPowered);
}

bool DeclarativeConnectionAgent::checkValidness()
{
    if (!connManagerInterface || !connManagerInterface->isValid()) {
        Q_EMIT errorReported("", "ConnectionAgent not available");
        return false;
    }

    return true;
}
