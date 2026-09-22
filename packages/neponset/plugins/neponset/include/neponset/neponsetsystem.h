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

#ifndef NEPONSETSYSTEM_H
#define NEPONSETSYSTEM_H

#include "scopy-neponset_export.h"
#include "neponsetapplier.h"
#include "neponsetbands.h"
#include "neponsetchipwidgets.h"
#include "neponsettopology.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QWidget>

#include <animatedrefreshbtn.h>
#include <tooltemplate.h>
#include <smallprogressbar.h>

namespace scopy {
class IIOWidgetGroup;
}

namespace scopy::neponset {

/**
 * The Neponset system tool: operating mode, band presets and board status.
 *
 * This is the page that replaces reading the band mapping spreadsheet by hand. Selecting a band
 * per direction per channel and pressing Apply performs the twenty-odd attribute writes that
 * row of the sheet describes, and the apply log underneath says exactly what happened - which
 * matters because the sheet and the drivers do not agree everywhere.
 */
class SCOPY_NEPONSET_EXPORT NeponsetSystem : public QWidget
{
	Q_OBJECT
public:
	NeponsetSystem(NeponsetTopology *topology, NeponsetApplier *applier, IIOWidgetGroup *group,
		       QWidget *parent = nullptr);
	~NeponsetSystem();

	/** Most recent apply log, as shown in the log pane. */
	QString applyLog() const;

	/** Mode a path reports being in: "bypass", "lut", "mixed" or "unknown". */
	QString detectedMode(int pathIndex) const;

	/** Band currently selected in the combo for a channel. Empty when out of range. */
	QString selectedRxBand(int pathIndex) const;
	QString selectedTxBand(int pathIndex) const;

Q_SIGNALS:
	void readRequested();

public Q_SLOTS:
	void refresh();

	/** Applies Bypass or LUT mode. pathIndex < 0 means every path. */
	void applyMode(NeponsetMode mode, int pathIndex = -1);

	/** Selects a band in the combo without applying it. Returns false for an unknown name. */
	bool selectRxBand(int pathIndex, const QString &bandName);
	bool selectTxBand(int pathIndex, const QString &bandName);

	/** Applies the bands currently selected for one channel, or for every channel. */
	void applyChannel(int pathIndex);
	void applyAllChannels();

private:
	QWidget *buildModeSection(QWidget *parent);
	QWidget *buildBandSection(QWidget *parent);
	QWidget *buildLogSection(QWidget *parent);
	QWidget *buildBoardSection(QWidget *parent);

	void updateLoSummary(int pathIndex);
	void updateModeSummary();
	void showResult(const ApplyResult &result, const QString &what);

	/** One channel's row of band controls. */
	struct ChannelRow
	{
		QComboBox *rxBand = nullptr;
		QComboBox *txBand = nullptr;
		QLabel *loSummary = nullptr;
		QPushButton *applyBtn = nullptr;
	};

	NeponsetTopology *m_topology = nullptr;
	NeponsetApplier *m_applier = nullptr;
	IIOWidgetGroup *m_group = nullptr;
	NeponsetChipWidgets *m_chips = nullptr;

	ToolTemplate *m_tool = nullptr;
	AnimatedRefreshBtn *m_refreshButton = nullptr;

	ChannelRow m_rows[PathCount];
	QComboBox *m_modePathSelect = nullptr;
	QLabel *m_modeSummary = nullptr;
	QCheckBox *m_deriveLoX3 = nullptr;
	QPlainTextEdit *m_log = nullptr;
	SmallProgressBar *m_status = nullptr;
};

} // namespace scopy::neponset

#endif // NEPONSETSYSTEM_H
