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

#ifndef NEPONSETPLUGIN_H
#define NEPONSETPLUGIN_H

#define SCOPY_PLUGIN_NAME NeponsetPlugin

#include "scopy-neponset_export.h"

#include <QObject>
#include <pluginbase/plugin.h>
#include <pluginbase/pluginbase.h>

namespace scopy {
class IIOWidgetGroup;
}

namespace scopy::neponset {

class NEPONSET_API;
class NeponsetApplier;
class NeponsetLo;
class NeponsetRx;
class NeponsetSystem;
class NeponsetTopology;
class NeponsetTx;

/**
 * Scopy plugin for the Neponset RF up/down converter module.
 *
 * Neponset carries four identical RF paths across 28 IIO devices: an ADMV1420 downconverter
 * with an ADMV8809 and an ADMV8827 tunable filter on receive, an ADMV1320 upconverter with an
 * ADMV8827 and an ADMV8909 on transmit, and one HMC7003 LO mixer shared by the receive and
 * transmit path of the same index, plus neponset_gpio and neponset_status on the board itself.
 *
 * That is presented as four tools, mirroring the hardware: a system tool for the operating
 * mode, the band presets and board status, and one tool each for the receive, transmit and LO
 * paths with a page per path.
 */
class SCOPY_NEPONSET_EXPORT NeponsetPlugin : public QObject, public PluginBase
{
	Q_OBJECT
	SCOPY_PLUGIN;

	friend class NEPONSET_API;

public:
	bool compatible(QString m_param, QString category) override;
	bool loadPage() override;
	bool loadIcon() override;
	void loadToolList() override;
	void unload() override;
	void initMetadata() override;
	QString description() override;
	QString displayName() override;

public Q_SLOTS:
	bool onConnect() override;
	bool onDisconnect() override;

private:
	void initApi();

	NEPONSET_API *m_api = nullptr;
	IIOWidgetGroup *m_widgetGroup = nullptr;
	NeponsetTopology *m_topology = nullptr;
	NeponsetApplier *m_applier = nullptr;

	// Owned by their ToolMenuEntry once setTool() is called; these are weak references kept
	// so the API can reach the tools.
	NeponsetSystem *m_system = nullptr;
	NeponsetRx *m_rx = nullptr;
	NeponsetTx *m_tx = nullptr;
	NeponsetLo *m_lo = nullptr;
};

} // namespace scopy::neponset

#endif // NEPONSETPLUGIN_H
