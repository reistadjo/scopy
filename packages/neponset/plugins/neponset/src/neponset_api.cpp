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

#include "neponset_api.h"
#include "neponsetbands.h"
#include "neponsetpathtool.h"
#include "neponsetplugin.h"
#include "neponsetsystem.h"
#include "neponsettopology.h"

#include <QLoggingCategory>
#include <iio-widgets/iiowidgetgroup.h>
#include <pluginbase/toolmenuentry.h>

Q_LOGGING_CATEGORY(CAT_NEPONSET_API, "NEPONSET_API")

using namespace scopy::neponset;

NEPONSET_API::NEPONSET_API(NeponsetPlugin *plugin)
	: ApiObject()
	, m_plugin(plugin)
{}

NEPONSET_API::~NEPONSET_API() {}

// --- private helpers ---

QString NEPONSET_API::readFromWidget(const QString &key)
{
	if(!m_plugin || !m_plugin->m_widgetGroup) {
		qWarning(CAT_NEPONSET_API) << "Widget group not available";
		return QString();
	}

	IIOWidget *widget = m_plugin->m_widgetGroup->get(key);
	if(!widget) {
		qWarning(CAT_NEPONSET_API) << "Widget not found for key:" << key;
		return QString();
	}

	QPair<QString, QString> result = widget->read();
	return result.first;
}

void NEPONSET_API::writeToWidget(const QString &key, const QString &value)
{
	if(!m_plugin || !m_plugin->m_widgetGroup) {
		qWarning(CAT_NEPONSET_API) << "Widget group not available";
		return;
	}

	IIOWidget *widget = m_plugin->m_widgetGroup->get(key);
	if(!widget) {
		qWarning(CAT_NEPONSET_API) << "Widget not found for key:" << key;
		return;
	}

	widget->writeAsync(value);
}

// --- tool management ---

QStringList NEPONSET_API::getTools()
{
	QStringList tools;
	if(!m_plugin) {
		return tools;
	}

	for(ToolMenuEntry *tool : m_plugin->m_toolList) {
		tools.append(tool->name());
	}
	return tools;
}

// --- topology ---

QStringList NEPONSET_API::getMissingDevices()
{
	if(!m_plugin || !m_plugin->m_topology) {
		return {};
	}
	return m_plugin->m_topology->missingDevices();
}

int NEPONSET_API::getPathCount() { return PathCount; }

// --- operating mode ---

void NEPONSET_API::setMode(const QString &mode, int pathIndex)
{
	if(!m_plugin || !m_plugin->m_system) {
		qWarning(CAT_NEPONSET_API) << "System tool not available";
		return;
	}

	const QString normalized = mode.trimmed().toLower();
	if(normalized == "bypass") {
		m_plugin->m_system->applyMode(NeponsetMode::Bypass, pathIndex);
	} else if(normalized == "lut") {
		m_plugin->m_system->applyMode(NeponsetMode::Lut, pathIndex);
	} else {
		qWarning(CAT_NEPONSET_API) << "Invalid mode:" << mode << "Valid: bypass, lut";
	}
}

QString NEPONSET_API::getMode(int pathIndex)
{
	if(!m_plugin || !m_plugin->m_system) {
		return QStringLiteral("unknown");
	}
	return m_plugin->m_system->detectedMode(pathIndex);
}

// --- band presets ---

QStringList NEPONSET_API::getRxBandNames()
{
	QStringList names;
	for(const RxBandConfig &band : rxBands()) {
		names.append(band.name);
	}
	return names;
}

QStringList NEPONSET_API::getTxBandNames()
{
	QStringList names;
	for(const TxBandConfig &band : txBands()) {
		names.append(band.name);
	}
	return names;
}

void NEPONSET_API::setRxBand(int pathIndex, const QString &bandName)
{
	if(!m_plugin || !m_plugin->m_system) {
		qWarning(CAT_NEPONSET_API) << "System tool not available";
		return;
	}
	if(!m_plugin->m_system->selectRxBand(pathIndex, bandName)) {
		qWarning(CAT_NEPONSET_API) << "Unknown RX band:" << bandName << "Valid:" << getRxBandNames();
	}
}

void NEPONSET_API::setTxBand(int pathIndex, const QString &bandName)
{
	if(!m_plugin || !m_plugin->m_system) {
		qWarning(CAT_NEPONSET_API) << "System tool not available";
		return;
	}
	if(!m_plugin->m_system->selectTxBand(pathIndex, bandName)) {
		qWarning(CAT_NEPONSET_API) << "Unknown TX band:" << bandName << "Valid:" << getTxBandNames();
	}
}

QString NEPONSET_API::getRxBand(int pathIndex)
{
	if(!m_plugin || !m_plugin->m_system) {
		return QString();
	}
	return m_plugin->m_system->selectedRxBand(pathIndex);
}

QString NEPONSET_API::getTxBand(int pathIndex)
{
	if(!m_plugin || !m_plugin->m_system) {
		return QString();
	}
	return m_plugin->m_system->selectedTxBand(pathIndex);
}

QString NEPONSET_API::getRxBandLo(const QString &bandName)
{
	const RxBandConfig *band = rxBandByName(bandName);
	return band ? QString::number(band->loGHz) : QString();
}

QString NEPONSET_API::getTxBandLo(const QString &bandName)
{
	const TxBandConfig *band = txBandByName(bandName);
	return band ? QString::number(band->loGHz) : QString();
}

void NEPONSET_API::applyChannel(int pathIndex)
{
	if(!m_plugin || !m_plugin->m_system) {
		qWarning(CAT_NEPONSET_API) << "System tool not available";
		return;
	}
	m_plugin->m_system->applyChannel(pathIndex);
}

void NEPONSET_API::applyAllChannels()
{
	if(!m_plugin || !m_plugin->m_system) {
		qWarning(CAT_NEPONSET_API) << "System tool not available";
		return;
	}
	m_plugin->m_system->applyAllChannels();
}

void NEPONSET_API::applyRxBand(int pathIndex, const QString &bandName)
{
	if(!m_plugin || !m_plugin->m_system) {
		qWarning(CAT_NEPONSET_API) << "System tool not available";
		return;
	}
	if(!m_plugin->m_system->selectRxBand(pathIndex, bandName)) {
		qWarning(CAT_NEPONSET_API) << "Unknown RX band:" << bandName << "Valid:" << getRxBandNames();
		return;
	}
	m_plugin->m_system->applyChannel(pathIndex);
}

void NEPONSET_API::applyTxBand(int pathIndex, const QString &bandName)
{
	if(!m_plugin || !m_plugin->m_system) {
		qWarning(CAT_NEPONSET_API) << "System tool not available";
		return;
	}
	if(!m_plugin->m_system->selectTxBand(pathIndex, bandName)) {
		qWarning(CAT_NEPONSET_API) << "Unknown TX band:" << bandName << "Valid:" << getTxBandNames();
		return;
	}
	m_plugin->m_system->applyChannel(pathIndex);
}

QString NEPONSET_API::getApplyLog()
{
	if(!m_plugin || !m_plugin->m_system) {
		return QString();
	}
	return m_plugin->m_system->applyLog();
}

// --- generic widget access ---

QStringList NEPONSET_API::getWidgetKeys()
{
	if(!m_plugin || !m_plugin->m_widgetGroup) {
		qWarning(CAT_NEPONSET_API) << "Widget group not available";
		return {};
	}
	return m_plugin->m_widgetGroup->keys();
}

QString NEPONSET_API::readWidget(const QString &key) { return readFromWidget(key); }

void NEPONSET_API::writeWidget(const QString &key, const QString &value) { writeToWidget(key, value); }

// --- utility ---

void NEPONSET_API::refresh()
{
	if(!m_plugin) {
		return;
	}

	if(m_plugin->m_system) {
		m_plugin->m_system->refresh();
	}
	if(m_plugin->m_rx) {
		m_plugin->m_rx->refresh();
	}
	if(m_plugin->m_tx) {
		m_plugin->m_tx->refresh();
	}
	if(m_plugin->m_lo) {
		m_plugin->m_lo->refresh();
	}
}

#include "moc_neponset_api.cpp"
