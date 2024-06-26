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
#include <QProcess>

#include "../../../connd/qconnectionagent.h"

#include <networkmanager.h>
#include <networktechnology.h>
#include <networkservice.h>


class Tst_connectionagent : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tst_onErrorReported();

private:
    QConnectionAgent agent;
};

void Tst_connectionagent::tst_onErrorReported()
{
    QSignalSpy spy(&agent, SIGNAL(errorReported(QString,QString)));
    agent.onErrorReported("test_path", "Test error");

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments;
    arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString("test_path"));
    QCOMPARE(arguments.at(1).toString(), QString("Test error"));

    agent.connectToType("test");
    QCOMPARE(spy.count(), 1);
    arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString(""));
    QCOMPARE(arguments.at(1).toString(), QString("Type not valid"));

}

QTEST_APPLESS_MAIN(Tst_connectionagent)

#include "tst_connectionagent.moc"
