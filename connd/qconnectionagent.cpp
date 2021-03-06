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

#include "qconnectionagent.h"
#include "connectiond_adaptor.h"

#include <connman-qt5/useragent.h>
#include <connman-qt5/networkmanager.h>
#include <connman-qt5/networktechnology.h>
#include <connman-qt5/networkservice.h>

#include <QtDBus/QDBusConnection>

#include <QObject>
#include <QSettings>

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"
#define CONND_SESSION_PATH = "/ConnectionSession"

QConnectionAgent::QConnectionAgent(QObject *parent) :
    QObject(parent),
    ua(0),
    netman(NetworkManagerFactory::createInstance()),
    currentNetworkState(QString()),
    isEthernet(false),
    connmanAvailable(false),
    tetheringWifiTech(0),
    tetheringEnabled(false),
    flightModeSuppression(false),
    scanTimeoutInterval(1),
    delayedTethering(false),
    valid(true)
{
    new ConnAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();

    if (!dbus.registerObject(CONND_PATH, this)) {
        qDebug() << "QConnectionAgent: Could not register object to path" << CONND_PATH;
        valid = false;
    }

    if (!dbus.registerService(CONND_SERVICE)) {
        qDebug() << "QConnectionAgent: could not register service" << CONND_SERVICE;
        valid = false;
    }

    connect(this, &QConnectionAgent::configurationNeeded, this, &QConnectionAgent::openConnectionDialog);

    connect(netman, &NetworkManager::availabilityChanged, this, &QConnectionAgent::connmanAvailabilityChanged);
    connect(netman, &NetworkManager::servicesListChanged, this, &QConnectionAgent::servicesListChanged);
    connect(netman, &NetworkManager::stateChanged, this, &QConnectionAgent::networkStateChanged);
    connect(netman, &NetworkManager::offlineModeChanged, this, &QConnectionAgent::offlineModeChanged);
    connect(netman, &NetworkManager::servicesChanged, this, [=]() {
        updateServices();
    });
    connect(netman, &NetworkManager::technologiesChanged, this, &QConnectionAgent::techChanged);

    QFile connmanConf("/etc/connman/main.conf");
    if (connmanConf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!connmanConf.atEnd()) {
            QString line = connmanConf.readLine();
            if (line.startsWith("DefaultAutoConnectTechnologies")) {
                QString token = line.section(" = ",1,1).simplified();
                techPreferenceList = token.split(",");
                break;
            }
        }
        connmanConf.close();
    }
    if (techPreferenceList.isEmpty())
        //ethernet,bluetooth,cellular,wifi is default
        techPreferenceList << "bluetooth" << "wifi" << "cellular" << "ethernet";

    connmanAvailable = QDBusConnection::systemBus().interface()->isServiceRegistered("net.connman");

    scanTimer = new QTimer(this);
    connect(scanTimer, &QTimer::timeout, this, &QConnectionAgent::scanTimeout);
    scanTimer->setSingleShot(true);
    if (connmanAvailable && valid)
        setup();
}

QConnectionAgent::~QConnectionAgent()
{
}

bool QConnectionAgent::isValid() const
{
    return valid;
}

// from useragent
void QConnectionAgent::onErrorReported(const QString &servicePath, const QString &error)
{
    if (shouldSuppressError(error, servicePath.contains("cellular")))
        return;

    if (!tetheringWifiTech) return;
    // Suppress errors when switching to tethering mode
    if ((delayedTethering || tetheringWifiTech->tethering()) && servicePath.contains(QStringLiteral("wifi")))
        return;

    qDebug() << "<<<<<<<<<<<<<<<<<<<<" << servicePath << error;
    Q_EMIT errorReported(servicePath, error);
}

// from useragent
void QConnectionAgent::onConnectionRequest()
{
    sendConnectReply("Suppress", 15);
    qDebug() << flightModeSuppression;
    bool okToRequest = true;
    Q_FOREACH (Service elem, orderedServicesList) {
        qDebug() << "checking" << elem.service->name() << elem.service->autoConnect();
        if (elem.service->autoConnect()) {
            okToRequest = false;
            break;
        }
    }
    if (!flightModeSuppression && okToRequest) {
        Q_EMIT connectionRequest();
    }
}

void QConnectionAgent::onBrowserRequested(const QString &servicePath, const QString &url)
{
    QString serviceName;
    for (const Service &elem : orderedServicesList) {
        if (elem.service->path() == servicePath) {
            serviceName = elem.service->name();
            break;
        }
    }
    Q_EMIT requestBrowser(url, serviceName);
}

void QConnectionAgent::sendConnectReply(const QString &in0, int in1)
{
    ua->sendConnectReply(in0, in1);
}

void QConnectionAgent::sendUserReply(const QVariantMap &input)
{
    qDebug() << Q_FUNC_INFO;
    ua->sendUserReply(input);
}

void QConnectionAgent::servicesListChanged(const QStringList &list)
{
    bool changed = false;

    Q_FOREACH(const QString &path, list) {
        if (orderedServicesList.indexOf(path) == -1) {
            //added
            qDebug() << Q_FUNC_INFO << "added" << path;
            changed = true;
            break;
        }
    }

    if (!changed)
        Q_FOREACH (Service elem, orderedServicesList) {
            if (list.indexOf(elem.path) == -1) {
                //removed
                qDebug() << Q_FUNC_INFO << "removed" << elem.path;
                changed = true;
                break;
            }
        }

    if (changed)
        updateServices();
}

void QConnectionAgent::serviceErrorChanged(const QString &error)
{
    NetworkService *service = static_cast<NetworkService *>(sender());
    if (shouldSuppressError(error, service->type() == QLatin1String("cellular")))
        return;

    Q_EMIT errorReported(service->path(), error);
}

void QConnectionAgent::serviceStateChanged(const QString &state)
{
    NetworkService *service = static_cast<NetworkService *>(sender());
    if (!service)
        return;
    qDebug() << state << service->name() << service->strength();
    qDebug() << "currentNetworkState" << currentNetworkState;

    if (state == "ready" && service->type() == "wifi"
            && !delayedTethering
            && netman->defaultRoute()->type() == "cellular") {
        netman->defaultRoute()->requestDisconnect();
    }

    if (state == "disconnect") {
        Q_EMIT connectionState(state, service->type());
    }

    if (!service->favorite() || !netman->getTechnology(service->type())
            || !netman->getTechnology(service->type())->powered()) {
        qDebug() << "not fav or not powered";
        return;
    }
    if (state == "disconnect") {
        ua->sendConnectReply("Clear");
    }
    if (state == "failure") {
        if (delayedTethering && service->type() == "cellular" && tetheringWifiTech->tethering()) {
            Q_EMIT tetheringFinished(false);
        }
    }

    if (delayedTethering && service->type() == "wifi" && state == "association") {
        service->requestDisconnect();
    }

    if (state == "online") {
        Q_EMIT connectionState(state, service->type());

        if (service->type() == "wifi" && delayedTethering) {
            netman->getTechnology(service->type())->setTethering(true);
        }
        if (service->type() == "cellular" && delayedTethering) {
            if (!tetheringWifiTech->tethering()) {
                tetheringWifiTech->setTethering(true);
            }
        }
    }
    //auto migrate
    if (state == "idle") {
        if (service->type() == "wifi" && delayedTethering) {
            netman->getTechnology(service->type())->setTethering(true);
        }
    } else {
        updateServices();
    }
    currentNetworkState = state;
    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    confFile.setValue("connected",currentNetworkState);
}

// from plugin/qml
void QConnectionAgent::connectToType(const QString &type)
{
    if (!netman)
        return;

    if (netman->technologyPathForType(type).isEmpty()) {
        Q_EMIT errorReported("","Type not valid");
        return;
    }

    // Connman is using "cellular" and "wifi" as part of the service path
    QString convType;
    if (type.contains("mobile")) {
        convType="cellular";
    } else if (type.contains("wlan")) {
        convType="wifi";
    } else {
        convType=type;
    }

    bool found = false;
    Q_FOREACH (Service elem, orderedServicesList) {
        if (elem.path.contains(convType)) {
            if (!isStateOnline(elem.service->state())) {
                if (elem.service->autoConnect()) {
                    qDebug() << "<<<<<<<<<<< requestConnect() >>>>>>>>>>>>";
                    elem.service->requestConnect();
                    return;
                } else if (!elem.path.contains("cellular")) {
                    // ignore cellular that are not on autoconnect
                    found = true;
                }
            } else {
                return;
            }
        }
    }

    // Can't connect to the service of a type that doesn't exist
    if (!found)
        return;

    // Substitute "wifi" with "wlan" for lipstick
    if (type.contains("wifi"))
        convType="wlan";

    Q_EMIT configurationNeeded(convType);
}

void QConnectionAgent::updateServices()
{
    qDebug() << Q_FUNC_INFO;
    ServiceList oldServices = orderedServicesList;
    orderedServicesList.clear();

    Q_FOREACH (const QString &tech,techPreferenceList) {
        QVector<NetworkService*> services = netman->getServices(tech);

        Q_FOREACH (NetworkService *serv, services) {
            const QString servicePath = serv->path();

            qDebug() << "known service:" << serv->name() << serv->strength();

            Service elem;
            elem.path = servicePath;
            elem.service = serv;
            orderedServicesList << elem;

            if (!oldServices.contains(servicePath)) {
                //new!
                qDebug() <<"new service"<< servicePath;

                QObject::connect(serv, &NetworkService::stateChanged,
                                 this, &QConnectionAgent::serviceStateChanged, Qt::UniqueConnection);
                QObject::connect(serv, &NetworkService::connectRequestFailed,
                                 this, &QConnectionAgent::serviceErrorChanged, Qt::UniqueConnection);
                QObject::connect(serv, &NetworkService::errorChanged,
                                 this, &QConnectionAgent::servicesError, Qt::UniqueConnection);
                QObject::connect(serv, &NetworkService::autoConnectChanged,
                                 this, &QConnectionAgent::serviceAutoconnectChanged, Qt::UniqueConnection);
            }
        }
    }
}

void QConnectionAgent::servicesError(const QString &errorMessage)
{
    if (errorMessage.isEmpty())
        return;
    NetworkService *serv = static_cast<NetworkService *>(sender());
    qDebug() << serv->name() << errorMessage;
    onErrorReported(serv->path(), errorMessage);
}

void QConnectionAgent::networkStateChanged(const QString &state)
{
    qDebug() << state;

    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    if (state != "online")
        confFile.setValue("connected","offline");
    else
        confFile.setValue("connected","online");

    if ((state == "online" && netman->defaultRoute()->type() == "cellular")
            || (state == "idle")) {

        if (tetheringWifiTech && tetheringWifiTech->powered()
                && !tetheringWifiTech->tethering())
                tetheringWifiTech->scan();
        // on gprs, scan wifi every scanTimeoutInterval minutes
        if (scanTimeoutInterval != 0)
            scanTimer->start(scanTimeoutInterval * 60 * 1000);
    }

    if (delayedTethering && state == "online") {

        if (tetheringWifiTech->tethering()) {
            if (netman->defaultRoute()->type() == "cellular") {
                delayedTethering = false;
                Q_EMIT tetheringFinished(true);
            }
        } else {
            tetheringWifiTech->setTethering(true);
        }
    }
}

void QConnectionAgent::connmanAvailabilityChanged(bool b)
{
    qDebug() << Q_FUNC_INFO;
    connmanAvailable = b;
    if (b) {
        setup();
        currentNetworkState = netman->state();
    } else {
        currentNetworkState = "error";
    }
}

void QConnectionAgent::setup()
{
    qDebug() << Q_FUNC_INFO
             << connmanAvailable;

    if (connmanAvailable) {
        qDebug() << Q_FUNC_INFO
                 << netman->state();
        delete ua;
        ua = new UserAgent(this);

        connect(ua, &UserAgent::connectionRequest, this, &QConnectionAgent::onConnectionRequest);
        connect(ua, &UserAgent::errorReported, this, &QConnectionAgent::onErrorReported);
        connect(ua, &UserAgent::userInputCanceled, this, &QConnectionAgent::userInputCanceled);
        connect(ua, &UserAgent::userInputRequested, this, &QConnectionAgent::userInputRequested);
        connect(ua, &UserAgent::browserRequested, this, &QConnectionAgent::onBrowserRequested);

        updateServices();
        offlineModeChanged(netman->offlineMode());

        QSettings confFile;
        confFile.beginGroup("Connectionagent");
        scanTimeoutInterval = confFile.value("scanTimerInterval", "1").toUInt(); //in minutes

        if (isStateOnline(netman->state())) {
            qDebug() << "default route type:" << netman->defaultRoute()->type();
            if (netman->defaultRoute()->type() == "ethernet")
                isEthernet = true;
            if (netman->defaultRoute()->type() == "cellular" && scanTimeoutInterval != 0)
                scanTimer->start(scanTimeoutInterval * 60 * 1000);

        }
        qDebug() << "config file says" << confFile.value("connected", "online").toString();
    }
}

void QConnectionAgent::technologyPowerChanged(bool powered)
{
    NetworkTechnology *tech = static_cast<NetworkTechnology *>(sender());
    if (tech->type() != "wifi")
        return;
    if (tetheringWifiTech)
        qDebug() << tetheringWifiTech->name() << powered;
    else
        qDebug() << "tetheringWifiTech is null";

    if (netman && powered && delayedTethering) {
        // wifi tech might not be ready, so delay this
        QTimer::singleShot(1000, this, &QConnectionAgent::setWifiTetheringEnabled);
    }
}

void QConnectionAgent::techChanged()
{
    if (!netman)
        return;
    if (netman->getTechnologies().isEmpty()) {
        knownTechnologies.clear();
    }
    if (!netman->getTechnology("wifi")) {
        delayedTethering = false;
        tetheringWifiTech = 0;
        return;
    }
    if (tetheringWifiTech) {
        tetheringEnabled = tetheringWifiTech->tethering();
    }

    Q_FOREACH(NetworkTechnology *technology,netman->getTechnologies()) {
        if (!knownTechnologies.contains(technology->path())) {
            knownTechnologies << technology->path();
            if (technology->type() == "wifi") {
                tetheringWifiTech = technology;
                connect(tetheringWifiTech, &NetworkTechnology::poweredChanged, this, &QConnectionAgent::technologyPowerChanged);
                connect(tetheringWifiTech, &NetworkTechnology::tetheringChanged,
                        this, &QConnectionAgent::techTetheringChanged, Qt::UniqueConnection);
            }
        } else {
            knownTechnologies.removeOne(technology->path());
        }
    }
}

bool QConnectionAgent::isStateOnline(const QString &state)
{
    if (state == "online" || state == "ready")
        return true;
    return false;
}

void QConnectionAgent::techTetheringChanged(bool on)
{
    qDebug() << on;
    tetheringEnabled = on;
    NetworkTechnology *technology = static_cast<NetworkTechnology *>(sender());

    if (on && delayedTethering && technology) {
        QVector <NetworkService *> services = netman->getServices("cellular");
        if (services.isEmpty())
            return;
        NetworkService* cellService = services.at(0);
        if (cellService) {
            if (cellService->state() == "idle"|| cellService->state() == "failure") {
                qDebug() << "<<<<<<<<<<< requestConnect() >>>>>>>>>>>>";
                cellService->requestConnect();
            } else if (cellService->connected()) {
                delayedTethering = false;
                Q_EMIT tetheringFinished(true);
            }
        } else {
            stopTethering();
        }
    }
}

void QConnectionAgent::offlineModeChanged(bool b)
{
    flightModeSuppression = b;
    if (b) {
        QTimer::singleShot(5 * 1000 * 60, this, &QConnectionAgent::flightModeDialogSuppressionTimeout); //5 minutes
    }
}

void QConnectionAgent::flightModeDialogSuppressionTimeout()
{
    flightModeSuppression = false;
}

void QConnectionAgent::serviceAutoconnectChanged(bool on)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());
    if (!service)
        return;
    qDebug() << service->path() << "AutoConnect is" << on;

    if (!on) {
        if (service->state() != "idle")
            service->requestDisconnect();
    }
}

void QConnectionAgent::scanTimeout()
{
    if (!tetheringWifiTech || tetheringWifiTech->tethering())
        return;

    if (tetheringWifiTech->powered() && !tetheringWifiTech->connected() && netman->defaultRoute()->type() != "wifi" ) {
        tetheringWifiTech->scan();
        qDebug() << "start scanner" << scanTimeoutInterval;
        if (scanTimeoutInterval != 0) {
            scanTimer->start(scanTimeoutInterval * 60 * 1000);
        }
    }
}

void QConnectionAgent::removeAllTypes(const QString &type)
{
    Q_FOREACH (Service elem, orderedServicesList) {
        if (elem.path.contains(type))
            orderedServicesList.remove(elem.path);
    }
}

bool QConnectionAgent::shouldSuppressError(const QString &error, bool cellular) const
{
    if (error.isEmpty())
        return true;
    if (error == QLatin1String("Operation aborted"))
        return true;
    if (error == QLatin1String("Already connected"))
        return true;
    if (error == QLatin1String("No carrier") && cellular) // Don't report cellular carrier lost.
        return true;
    if (error == QLatin1String("connect-failed") && cellular && netman->offlineMode()) // Suppress errors when switching to offline mode
        return true;
    if (error == QLatin1String("In progress") || error.contains(QLatin1String("Method"))) // Catch dbus errors and discard
        return true;

    return false;
}

void QConnectionAgent::openConnectionDialog(const QString &type)
{
    // open Connection Selector
    QDBusInterface *connSelectorInterface = new QDBusInterface(QStringLiteral("com.jolla.lipstick.ConnectionSelector"),
                                                               QStringLiteral("/"),
                                                               QStringLiteral("com.jolla.lipstick.ConnectionSelectorIf"),
                                                               QDBusConnection::sessionBus(),
                                                               this);

    connSelectorInterface->connection().connect(QStringLiteral("com.jolla.lipstick.ConnectionSelector"),
                                                QStringLiteral("/"),
                                                QStringLiteral("com.jolla.lipstick.ConnectionSelectorIf"),
                                                QStringLiteral("connectionSelectorClosed"),
                                                this,
                                                SLOT(connectionSelectorClosed(bool)));

    QList<QVariant> args;
    args.append(type);
    QDBusMessage reply = connSelectorInterface->callWithArgumentList(QDBus::NoBlock,
                                                                     QStringLiteral("openConnection"), args);
}

void QConnectionAgent::startTethering(const QString &type)
{
    if (!netman | (type != "wifi")) { //we only support wifi for now
        Q_EMIT tetheringFinished(false);
        return;
    }

    NetworkTechnology *tetherTech = netman->getTechnology(type);
    if (!tetherTech) {
        Q_EMIT tetheringFinished(false);
        return;
    }

    QVector <NetworkService *> services = netman->getServices("cellular");
    if (services.isEmpty()) {
        Q_EMIT tetheringFinished(false);
        return;
    }

    NetworkService *cellService = services.at(0);
    if (!cellService || netman->offlineMode()) {
        Q_EMIT tetheringFinished(false);
        return;
    }

    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    bool cellConnected = cellService->connected();
    bool cellAutoconnect = cellService->autoConnect();

    // save cellular connection state
    confFile.setValue("tetheringCellularConnected",cellConnected);
    confFile.setValue("tetheringCellularAutoconnect",cellAutoconnect);

    bool techPowered = tetherTech->powered();

    // save wifi powered state
    confFile.setValue("tetheringTechPowered",techPowered);
    confFile.setValue("tetheringType",type);

    delayedTethering = true;
    tetheringWifiTech = tetherTech;

    if (!techPowered) {
        tetherTech->setPowered(true);
    } else {
        tetherTech->setTethering(true);
    }
}

void QConnectionAgent::stopTethering(bool keepPowered)
{
    delayedTethering = false;
    QSettings confFile;
    confFile.beginGroup("Connectionagent");

    NetworkTechnology *tetherTech = netman->getTechnology(confFile.value("tetheringType","wifi").toString());
    if (tetherTech && tetherTech->tethering()) {
        tetherTech->setTethering(false);
    }
    bool b = confFile.value("tetheringCellularConnected").toBool();
    bool ab = confFile.value("tetheringCellularAutoconnect").toBool();

    Q_FOREACH (Service elem, orderedServicesList) {
        if (elem.path.contains("cellular")) {
            if (isStateOnline(elem.service->state())) {
                qDebug() << "disconnect mobile data";
                if (!b)
                    elem.service->requestDisconnect();
                if (!ab)
                    elem.service->setAutoConnect(false);
            }
        }
    }

    b = confFile.value("tetheringTechPowered").toBool();
    if (!b && tetherTech && !keepPowered) {
        tetherTech->setPowered(false);
    }
    Q_EMIT tetheringFinished(false);
}

void QConnectionAgent::setWifiTetheringEnabled()
{
    if (tetheringWifiTech) {
        qDebug() << "set tethering";
        tetheringWifiTech->setTethering(delayedTethering);
    }
}
