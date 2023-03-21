/****************************************************************************
**
** Copyright (C) 2014-2017 Jolla Ltd
** Contact: lorn.potter@gmail.com
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

#ifndef QCONNECTIONAGENT_H
#define QCONNECTIONAGENT_H

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QLoggingCategory>

class UserAgent;
class NetworkManager;
class NetworkService;
class NetworkTechnology;
class QTimer;

class QConnectionAgent : public QObject
{
    Q_OBJECT

public:
    explicit QConnectionAgent(QObject *parent = 0);
    ~QConnectionAgent();

    bool isValid() const;

Q_SIGNALS:
    void userInputRequested(const QString &servicePath, const QVariantMap &fields);
    void userInputCanceled();
    void errorReported(const QString &servicePath, const QString &error);
    void connectionRequest();
    void configurationNeeded(const QString &type);
    void connectionState(const QString &state, const QString &type);
    void connectNow(const QString &path);

    void requestBrowser(const QString &url, const QString &serviceName);
    void wifiTetheringFinished(bool);
    void bluetoothTetheringFinished(bool);

public Q_SLOTS:
    void onErrorReported(const QString &servicePath, const QString &error);

    void onConnectionRequest();
    void onBrowserRequested(const QString &url, const QString &serviceName);

    void sendConnectReply(const QString &in0, int in1);
    void sendUserReply(const QVariantMap &input);

    void connectToType(const QString &type);

    void startTethering(const QString &type);
    void stopTethering(const QString &type, bool keepPowered = false);

private:

    class Service
    {
    public:
        QString path;
        NetworkService *service;

        bool operator==(const Service &other) const {
            return other.path == path;
        }
    };

    class ServiceList : public QVector<Service>
    {
    public:
        int indexOf(const QString &path, int from = 0) const {
            Service key;
            key.path = path;
            return QVector<Service>::indexOf(key, from);
        }

        bool contains(const QString &path) const {
            Service key;
            key.path = path;
            return QVector<Service>::indexOf(key) >= 0;
        }

        void remove(const QString &path) {
            Service key;
            key.path = path;
            int pos = QVector<Service>::indexOf(key);
            if (pos >= 0)
                QVector<Service>::remove(pos);
        }
    };

    void setup();
    void updateServices();
    bool isStateOnline(const QString &state);
    void removeAllTypes(const QString &type);

    bool shouldSuppressError(const QString &error, bool cellular) const;

    UserAgent *ua;
    NetworkManager *netman;
    QString currentNetworkState;
    ServiceList orderedServicesList;
    QStringList techPreferenceList;
    bool isEthernet;
    bool connmanAvailable;

    NetworkTechnology *tetheringWifiTech;
    NetworkTechnology *tetheringBtTech;
    // Turn on Wifi tethering when the tech is next powered up. Wifi will be turned on
    // when this is enabled, so it won't be long. This gets reset once tethering is on
    // so will not be restored after flight mode or power off
    bool tetherWifiWhenPowered;
    // Turn on Bluetooth tethering whenever the BT adaptor is powered on. This will not
    // get reset, and is stored persistently in config, so when the option is enabled
    // tethering will always be started when BT is powered on.
    bool tetherBtWhenPowered;
    bool flightModeSuppression;
    uint scanTimeoutInterval;

    QTimer *scanTimer;
    QStringList knownTechnologies;
    bool valid;

private slots:
    void serviceErrorChanged(const QString &error);
    void serviceStateChanged(const QString &state);
    void networkStateChanged(const QString &state);

    void connmanAvailabilityChanged(bool b);
    void servicesError(const QString &);
    void technologyPowerChanged(bool);
    void techChanged();

    void servicesListChanged(const QStringList &);
    void offlineModeChanged(bool);
    void flightModeDialogSuppressionTimeout();

    void serviceAutoconnectChanged(bool);
    void scanTimeout();
    void techTetheringChanged(bool b);

    void openConnectionDialog(const QString &type);
    void enableWifiTethering();
    void enableBtTethering();
};

#endif // QCONNECTIONAGENT_H
