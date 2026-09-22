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

#ifndef NEPONSET_API_H
#define NEPONSET_API_H

#include "scopy-neponset_export.h"

#include <QString>
#include <QStringList>
#include <pluginbase/apiobject.h>

namespace scopy::neponset {

class NeponsetPlugin;

/**
 * Scripting surface for the Neponset plugin.
 *
 * Registered as "neponset" in the Scopy JS environment. The generic widget accessors reach
 * every IIOWidget the plugin built, which is most of the control surface; the typed methods
 * cover the things a script cannot reasonably drive attribute by attribute - the two operating
 * modes and the band presets.
 */
class SCOPY_NEPONSET_EXPORT NEPONSET_API : public ApiObject
{
	Q_OBJECT
public:
	explicit NEPONSET_API(NeponsetPlugin *plugin);
	~NEPONSET_API();

	// --- tool management ---
	Q_INVOKABLE QStringList getTools();

	// --- topology ---

	/** Device names that the plugin expected but did not find in the context. */
	Q_INVOKABLE QStringList getMissingDevices();
	/** Number of RF paths the plugin looks for. */
	Q_INVOKABLE int getPathCount();

	// --- operating mode ---

	/** "bypass" or "lut", case insensitive. pathIndex < 0 applies to every path. */
	Q_INVOKABLE void setMode(const QString &mode, int pathIndex = -1);

	/** Mode the hardware reports: "bypass", "lut", "mixed" or "unknown". */
	Q_INVOKABLE QString getMode(int pathIndex = 0);

	// --- band presets ---

	/** Band names available on the RX and TX sheets, in spreadsheet order. */
	Q_INVOKABLE QStringList getRxBandNames();
	Q_INVOKABLE QStringList getTxBandNames();

	/** Selects a band in the UI without applying it. */
	Q_INVOKABLE void setRxBand(int pathIndex, const QString &bandName);
	Q_INVOKABLE void setTxBand(int pathIndex, const QString &bandName);
	Q_INVOKABLE QString getRxBand(int pathIndex);
	Q_INVOKABLE QString getTxBand(int pathIndex);

	/** LO frequency in GHz that a band implies, as a string. Empty for an unknown band. */
	Q_INVOKABLE QString getRxBandLo(const QString &bandName);
	Q_INVOKABLE QString getTxBandLo(const QString &bandName);

	/** Applies the selected bands. */
	Q_INVOKABLE void applyChannel(int pathIndex);
	Q_INVOKABLE void applyAllChannels();

	/** Select and apply in one call, the usual scripted sequence. */
	Q_INVOKABLE void applyRxBand(int pathIndex, const QString &bandName);
	Q_INVOKABLE void applyTxBand(int pathIndex, const QString &bandName);

	/** Full apply log text, so a test can assert that nothing was skipped or failed. */
	Q_INVOKABLE QString getApplyLog();

	// --- generic widget access ---

	Q_INVOKABLE QStringList getWidgetKeys();
	Q_INVOKABLE QString readWidget(const QString &key);
	Q_INVOKABLE void writeWidget(const QString &key, const QString &value);

	// --- utility ---

	Q_INVOKABLE void refresh();

private:
	QString readFromWidget(const QString &key);
	void writeToWidget(const QString &key, const QString &value);

	NeponsetPlugin *m_plugin = nullptr;
};

} // namespace scopy::neponset

#endif // NEPONSET_API_H
