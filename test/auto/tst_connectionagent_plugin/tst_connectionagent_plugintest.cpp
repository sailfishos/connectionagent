/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
** Contact: lorn.potter@jollamobile.com
**
**
** $QT_BEGIN_LICENSE:LGPL$
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QString>
#include <QtTest>
#include "../../../connectionagentplugin/declarativeconnectionagent.h"

#include <networkmanager.h>
#include <networktechnology.h>
#include <networkservice.h>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class Tst_connectionagent_pluginTest : public QObject
{
    Q_OBJECT

public:
    Tst_connectionagent_pluginTest();
    ~Tst_connectionagent_pluginTest();

private Q_SLOTS:
    void testRequestConnection_data();
    void testRequestConnection();

    void testUserInputRequested_data();
    void testUserInputRequested();
    void testErrorReported();

    void tst_tethering();

private:
    DeclarativeConnectionAgent *plugin;
    QSharedPointer<NetworkManager> netman;
};

Tst_connectionagent_pluginTest::Tst_connectionagent_pluginTest()
{
    plugin = new DeclarativeConnectionAgent(this);
    netman = NetworkManager::sharedInstance();
    QTest::qWait(5000);
}

Tst_connectionagent_pluginTest::~Tst_connectionagent_pluginTest()
{
}

void Tst_connectionagent_pluginTest::testRequestConnection_data()
{
    QTest::addColumn<QString>("tech");
    QTest::newRow("wifi") << "wifi";
    QTest::newRow("cellular") << "cellular";
}

void Tst_connectionagent_pluginTest::testRequestConnection()
{
    QFETCH(QString, tech);

    QString techPath = netman->technologyPathForType(tech);
    NetworkTechnology netTech;
    netTech.setPath(techPath);

    QVector<NetworkService*> wifiServices = netman->getServices("wifi");
    for (int i = 0; i < wifiServices.count(); i++) {
        if (wifiServices[i]->autoConnect())
            wifiServices[i]->setAutoConnect(false);
        if (wifiServices[i]->serviceState() == NetworkService::OnlineState
                || wifiServices[i]->serviceState() == NetworkService::ReadyState) {
            wifiServices[i]->requestDisconnect();
            //autoconnect disables the requestConnect signal
        }
    }

    QSignalSpy spy2(plugin, SIGNAL(connectionRequest()));

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(QUrl("http://llornkcor.com")));
    if (reply->error()) {
        qDebug() << reply->error();
    }
    QTest::qWait(2000);
    QCOMPARE(spy2.count(), 1);
    plugin->sendConnectReply("Clear", 0);
}

void Tst_connectionagent_pluginTest::testErrorReported()
{
    QSignalSpy spy(plugin, SIGNAL(errorReported(QString, QString)));
    plugin->connectToType("test");
    QTest::qWait(2000);
    QCOMPARE(spy.count(),1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(1).toString(), QString("Type not valid"));
}

void Tst_connectionagent_pluginTest::testUserInputRequested_data()
{
    testRequestConnection_data();
}

void Tst_connectionagent_pluginTest::testUserInputRequested()
{
    QFETCH(QString, tech);

    QString techPath = netman->technologyPathForType(tech);
    NetworkTechnology netTech;
    netTech.setPath(techPath);

    QSignalSpy spy_userInput(plugin, SIGNAL(userInputRequested(QString,QVariantMap)));

    QVector<NetworkService*> wifiServices = netman->getServices("wifi");
    for (int i = 0; i < wifiServices.count(); i++) {
        if(wifiServices[i]->favorite()) {
            //favorite disables the need for user input
            wifiServices[i]->remove();
        }
        if (wifiServices[i]->autoConnect())
            wifiServices[i]->setAutoConnect(false);
        if (wifiServices[i]->serviceState() == NetworkService::IdleState) {
            wifiServices[i]->requestConnect();
            break;
        }
    }

    QTest::qWait(2000);
    QCOMPARE(spy_userInput.count(), 1);
    QVariantMap map;
    plugin->sendUserReply(map); //cancel
}

void Tst_connectionagent_pluginTest::tst_tethering()
{
    NetworkService *wlanService = 0;
    NetworkService *mobiledataService;

    QVector <NetworkService *>wifiServices = netman->getServices("wifi");
    QVERIFY(wifiServices.count() > 0);

    Q_FOREACH (wlanService, wifiServices) {
        if (wlanService->autoConnect()) {
            break;
        }
    }
    bool wlanOnline = wlanService->connected();

    QVector <NetworkService *>cellServices = netman->getServices("cellular");
    QVERIFY(cellServices.count() > 0);
    mobiledataService = cellServices.at(0);

    { // wifi tethering test
        bool mdOnline = mobiledataService->connected();
        bool mdAutoconnect = mobiledataService->autoConnect();
    
        QSignalSpy wifiSpy(plugin, SIGNAL(wifiTetheringFinished(bool)));
        plugin->startTethering("wifi");
    
        QVERIFY(wifiSpy.isValid());
        QVERIFY(wifiSpy.wait(7000));
    
        QCOMPARE(wifiSpy.count(), 1);
        QList<QVariant> arguments;
        arguments = wifiSpy.takeFirst();
        QCOMPARE(arguments.at(0).toBool(), true);
    
        QTest::qWait(5000);
    
        QVERIFY(mobiledataService->serviceState() == NetworkService::OnlineState);
    
        plugin->stopTethering("wifi");
        QTest::qWait(2500);
    
        QCOMPARE(wifiSpy.count(), 1);
        arguments = wifiSpy.takeFirst();
        QCOMPARE(arguments.at(0).toBool(), false);
    
        QTest::qWait(5000);
    
        QVERIFY(wlanService->connected() == wlanOnline);
        QVERIFY(mobiledataService->connected() == mdOnline);
        QVERIFY(mobiledataService->autoConnect() == mdAutoconnect);
    }

    { // bt tethering test
        QSignalSpy btSpy(plugin, SIGNAL(btTetheringFinished(bool)));
        plugin->startTethering("bluetooth");
    
        QVERIFY(btSpy.isValid());
        QVERIFY(btSpy.wait(7000));
    
        QCOMPARE(btSpy.count(), 1);
        QList<QVariant> arguments;
        arguments = btSpy.takeFirst();
        QCOMPARE(arguments.at(0).toBool(), true);
    
        plugin->stopTethering("bluetooth");
        QTest::qWait(2500);
    
        QCOMPARE(btSpy.count(), 1);
        arguments = btSpy.takeFirst();
        QCOMPARE(arguments.at(0).toBool(), false);
    }
//    plugin->startTethering("wifi");


//    plugin->stopTethering();

//    QCOMPARE(spy.count(),1);
//    arguments = spy.takeFirst();
//    QCOMPARE(arguments.at(0).toBool(), false);

//    NetworkService *cellServices = netman->getServices("cellular").at(0);
//    QVERIFY(cellServices->serviceState() == NetworkService::IdleState);
}

QTEST_MAIN(Tst_connectionagent_pluginTest)

#include "tst_connectionagent_plugintest.moc"
