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

#include "connectionagentplugin_plugin.h"
#include "declarativeconnectionagent.h"

void ConnectionagentpluginPlugin::registerTypes(const char *uri)
{
    // @uri com.jolla.connection
    qmlRegisterType<DeclarativeConnectionAgent>(uri, 1, 0, "ConnectionAgent");
}
