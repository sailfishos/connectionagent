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

#include <connman-qt5/networkmanager.h>
#include <connman-qt5/networktechnology.h>
#include <connman-qt5/networkservice.h>

#include <qobject.h>

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"

DeclarativeConnectionAgent::DeclarativeConnectionAgent(QObject *parent):
    QObject(parent),
    connManagerInterface(0)
{
    connectiondWatcher = new QDBusServiceWatcher(CONND_SERVICE,QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForRegistration |
            QDBusServiceWatcher::WatchForUnregistration, this);

    connect(connectiondWatcher, SIGNAL(serviceRegistered(QString)),
            this, SLOT(connectToConnectiond(QString)));
    connect(connectiondWatcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(connectiondUnregistered(QString)));

    connectToConnectiond();
}

DeclarativeConnectionAgent::~DeclarativeConnectionAgent()
{
}

void DeclarativeConnectionAgent::connectToConnectiond(QString)
{
    if (connManagerInterface) {
        delete connManagerInterface;
        connManagerInterface = 0;
    }

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
    connect(connManagerInterface,SIGNAL(connectionRequest()),
            this,SLOT(onConnectionRequested()));
    connect(connManagerInterface,SIGNAL(configurationNeeded(QString)),
            this,SIGNAL(configurationNeeded(QString)));

    connect(connManagerInterface,SIGNAL(userInputCanceled()),
            this,SIGNAL(userInputCanceled()));

    connect(connManagerInterface, SIGNAL(errorReported(QString, QString)),
            this, SLOT(onErrorReported(QString, QString)));

    connect(connManagerInterface,SIGNAL(connectionState(QString, QString)),
                     this,SLOT(onConnectionState(QString, QString)));

    connect(connManagerInterface,SIGNAL(requestBrowser(QString)),
                     this,SLOT(onRequestBrowser(QString)));

    connect(connManagerInterface,SIGNAL(userInputRequested(QString,QVariantMap)),
                     this,SLOT(onUserInputRequested(QString,QVariantMap)), Qt::UniqueConnection);

    connect(connManagerInterface,SIGNAL(tetheringFinished(bool)),
            this,SLOT(onTetheringFinished(bool)));
}

void DeclarativeConnectionAgent::sendUserReply(const QVariantMap &input)
{
    if (!connManagerInterface || !connManagerInterface->isValid()) {
        Q_EMIT errorReported("","ConnectionAgent not available");
        return;
    }
    QDBusPendingReply<> reply = connManagerInterface->sendUserReply(input);
    reply.waitForFinished();
    if (reply.isError()) {
        qDebug() << Q_FUNC_INFO << reply.error().message();
        Q_EMIT errorReported("",reply.error().message());
    }
}

void DeclarativeConnectionAgent::sendConnectReply(const QString &replyMessage, int timeout)
{
    if (!connManagerInterface || !connManagerInterface->isValid()) {
        Q_EMIT errorReported("","ConnectionAgent not available");
        return;
    }
    connManagerInterface->sendConnectReply(replyMessage,timeout);
}

void DeclarativeConnectionAgent::connectToType(const QString &type)
{
    if (!connManagerInterface || !connManagerInterface->isValid()) {
        Q_EMIT errorReported("","ConnectionAgent not available");
        return;
    }

    connManagerInterface->connectToType(type);
}

void DeclarativeConnectionAgent::onErrorReported(const QString &servicePath, const QString &error)
{
    Q_EMIT errorReported(servicePath, error);
}

void DeclarativeConnectionAgent::onRequestBrowser(const QString &url)
{
    qDebug() << Q_FUNC_INFO <<url;
    Q_EMIT browserRequested(url);
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

void DeclarativeConnectionAgent::onConnectionRequested()
{
    qDebug() << Q_FUNC_INFO;
    Q_EMIT connectionRequest();
}

void DeclarativeConnectionAgent::connectiondUnregistered(QString)
{
    if (connManagerInterface) {
        delete connManagerInterface;
        connManagerInterface = 0;
    }
}

void DeclarativeConnectionAgent::onConnectionState(const QString &state, const QString &type)
{
    qDebug() << Q_FUNC_INFO << state;
    Q_EMIT connectionState(state, type);
}

void DeclarativeConnectionAgent::startTethering(const QString &type)
{
    connManagerInterface->startTethering(type);
}

void DeclarativeConnectionAgent::onTetheringFinished(bool success)
{
    Q_EMIT tetheringFinished(success);
}

void DeclarativeConnectionAgent::stopTethering(bool keepPowered)
{
    connManagerInterface->stopTethering(keepPowered);
}