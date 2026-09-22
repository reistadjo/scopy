/*
 * Copyright (c) 2025 Analog Devices Inc.
 *
 * This file is part of Scopy
 * (see https://www.github.com/analogdevicesinc/scopy).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "neponsetplugin.h"
#include "neponset_api.h"
#include "neponsetapplier.h"
#include "neponsetpathtool.h"
#include "neponsetsystem.h"
#include "neponsettopology.h"
#include "scopy-neponset_config.h"

#include <QLabel>
#include <QLoggingCategory>

#include <deviceiconbuilder.h>
#include <iio-widgets/iiowidgetgroup.h>
#include <iioutil/connectionprovider.h>
#include <pluginbase/scopyjs.h>
#include <style.h>

Q_LOGGING_CATEGORY(CAT_NEPONSETPLUGIN, "NeponsetPlugin")

using namespace scopy;
using namespace scopy::neponset;

bool NeponsetPlugin::compatible(QString m_param, QString category)
{
	qDebug(CAT_NEPONSETPLUGIN) << "Checking Neponset compatibility";

	Connection *conn = ConnectionProvider::open(m_param);
	if(!conn) {
		qWarning(CAT_NEPONSETPLUGIN) << "No context available for" << m_param;
		return false;
	}

	const bool ret = NeponsetTopology::isNeponsetContext(conn->context());
	ConnectionProvider::close(m_param);

	return ret;
}

bool NeponsetPlugin::loadPage() { return false; }

bool NeponsetPlugin::loadIcon()
{
	QLabel *logo = new QLabel();
	QPixmap pixmap(":/gui/icons/scopy-default/icons/logo_analog.svg");
	const int pixmapHeight = 14;
	pixmap = pixmap.scaledToHeight(pixmapHeight, Qt::SmoothTransformation);
	logo->setPixmap(pixmap);

	QLabel *footer = new QLabel("NEPONSET");
	Style::setStyle(footer, style::properties::label::deviceIcon, true);

	m_icon = DeviceIconBuilder().shape(DeviceIconBuilder::SQUARE).headerWidget(logo).footerWidget(footer).build();

	return true;
}

void NeponsetPlugin::loadToolList()
{
	const QString icon =
		":/gui/icons/" + Style::getAttribute(json::theme::icon_theme_folder) + "/icons/gear_wheel.svg";

	m_toolList.append(SCOPY_NEW_TOOLMENUENTRY("neponsetSystemTool", "Neponset", icon));
	m_toolList.append(SCOPY_NEW_TOOLMENUENTRY("neponsetRxTool", "Neponset RX", icon));
	m_toolList.append(SCOPY_NEW_TOOLMENUENTRY("neponsetTxTool", "Neponset TX", icon));
	m_toolList.append(SCOPY_NEW_TOOLMENUENTRY("neponsetLoTool", "Neponset LO", icon));
}

void NeponsetPlugin::unload() {}

QString NeponsetPlugin::description() { return NEPONSET_PLUGIN_DESCRIPTION; }

QString NeponsetPlugin::displayName() { return NEPONSET_PLUGIN_DISPLAY_NAME; }

bool NeponsetPlugin::onConnect()
{
	Connection *conn = ConnectionProvider::open(m_param);
	if(!conn) {
		qWarning(CAT_NEPONSETPLUGIN) << "No context available for" << m_param;
		return false;
	}

	m_widgetGroup = new IIOWidgetGroup(this);
	m_topology = new NeponsetTopology(conn->context());
	m_applier = new NeponsetApplier(m_topology, this);

	m_system = new NeponsetSystem(m_topology, m_applier, m_widgetGroup);
	m_rx = new NeponsetRx(m_topology, m_widgetGroup);
	m_tx = new NeponsetTx(m_topology, m_widgetGroup);
	m_lo = new NeponsetLo(m_topology, m_widgetGroup);

	// A preset writes straight to libiio, so every tool has to be told to re-read afterwards
	// or the widgets keep showing pre-apply values.
	connect(m_applier, &NeponsetApplier::configurationChanged, m_rx, &NeponsetPathTool::readRequested);
	connect(m_applier, &NeponsetApplier::configurationChanged, m_tx, &NeponsetPathTool::readRequested);
	connect(m_applier, &NeponsetApplier::configurationChanged, m_lo, &NeponsetPathTool::readRequested);

	QWidget *tools[] = {m_system, m_rx, m_tx, m_lo};
	for(int i = 0; i < m_toolList.size() && i < 4; ++i) {
		m_toolList[i]->setTool(tools[i]);
		m_toolList[i]->setEnabled(true);
		m_toolList[i]->setRunBtnVisible(false);
	}

	initApi();
	return true;
}

bool NeponsetPlugin::onDisconnect()
{
	if(m_api) {
		ScopyJS::GetInstance()->unregisterApi(m_api);
		delete m_api;
		m_api = nullptr;
	}

	for(auto &tool : m_toolList) {
		tool->setEnabled(false);
		tool->setRunning(false);
		tool->setRunBtnVisible(false);
		QWidget *w = tool->tool();
		if(w) {
			tool->setTool(nullptr);
			delete w;
		}
	}
	m_system = nullptr;
	m_rx = nullptr;
	m_tx = nullptr;
	m_lo = nullptr;

	if(m_applier) {
		delete m_applier;
		m_applier = nullptr;
	}

	if(m_topology) {
		delete m_topology;
		m_topology = nullptr;
	}

	if(m_widgetGroup) {
		delete m_widgetGroup;
		m_widgetGroup = nullptr;
	}

	ConnectionProvider::close(m_param);
	return true;
}

void NeponsetPlugin::initApi()
{
	m_api = new NEPONSET_API(this);
	m_api->setObjectName("neponset");
	ScopyJS::GetInstance()->registerApi(m_api);
}

void NeponsetPlugin::initMetadata()
{
	loadMetadata(
		R"plugin(
	{
	   "priority":100,
	   "category":[
	      "iio"
	   ],
	   "exclude":["m2kplugin"]
	}
)plugin");
}
