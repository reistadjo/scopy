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

#ifndef NEPONSETPATHTOOL_H
#define NEPONSETPATHTOOL_H

#include "scopy-neponset_export.h"
#include "neponsetchipwidgets.h"
#include "neponsettopology.h"

#include <QButtonGroup>
#include <QStackedWidget>
#include <QWidget>

#include <animatedrefreshbtn.h>
#include <tooltemplate.h>

namespace scopy {
class IIOWidgetGroup;
}

namespace scopy::neponset {

/**
 * Shared shell for the three per-path tools.
 *
 * Neponset has four identical RF paths, so RX, TX and LO all want the same thing: a
 * ToolTemplate, a refresh button, and one page per path selected by a row of buttons. That is
 * the standard N-page pattern in this repo (AD936X Advanced and ADRV9009 Advanced both do it
 * with a QStackedWidget plus an exclusive QButtonGroup in the top container), so the only thing
 * a subclass has to supply is the contents of one page.
 */
class SCOPY_NEPONSET_EXPORT NeponsetPathTool : public QWidget
{
	Q_OBJECT
public:
	NeponsetPathTool(const QString &buttonPrefix, int pathCount, QWidget *parent = nullptr);
	~NeponsetPathTool();

Q_SIGNALS:
	/** Fanned out to every IIOWidget on every page. */
	void readRequested();

public Q_SLOTS:
	/** Re-read every widget. Used by the refresh button and by the plugin after a preset. */
	void refresh();

protected:
	/** Page contents for one path. Return a widget; it gets wrapped in a scroll area. */
	virtual QWidget *createPathPage(int index, QWidget *parent) = 0;

	/**
	 * Builds the pages and the selector buttons.
	 *
	 * Call this at the end of the subclass constructor - it cannot run from this base
	 * constructor because createPathPage() is pure virtual until the subclass is built.
	 */
	void buildPages();

	ToolTemplate *toolTemplate() const { return m_tool; }

private:
	QString m_buttonPrefix;
	int m_pathCount = 0;

	ToolTemplate *m_tool = nullptr;
	QStackedWidget *m_stack = nullptr;
	QButtonGroup *m_buttons = nullptr;
	AnimatedRefreshBtn *m_refreshButton = nullptr;
};

/** RX paths: ADMV1420 downconverter, ADMV8809 low band filter, ADMV8827 high band filter. */
class SCOPY_NEPONSET_EXPORT NeponsetRx : public NeponsetPathTool
{
	Q_OBJECT
public:
	NeponsetRx(NeponsetTopology *topology, IIOWidgetGroup *group, QWidget *parent = nullptr);

protected:
	QWidget *createPathPage(int index, QWidget *parent) override;

private:
	NeponsetTopology *m_topology = nullptr;
	NeponsetChipWidgets *m_chips = nullptr;
};

/** TX paths: ADMV1320 upconverter, ADMV8827 high band filter, ADMV8909 wideband filter. */
class SCOPY_NEPONSET_EXPORT NeponsetTx : public NeponsetPathTool
{
	Q_OBJECT
public:
	NeponsetTx(NeponsetTopology *topology, IIOWidgetGroup *group, QWidget *parent = nullptr);

protected:
	QWidget *createPathPage(int index, QWidget *parent) override;

private:
	NeponsetTopology *m_topology = nullptr;
	NeponsetChipWidgets *m_chips = nullptr;
};

/** LO paths: HMC7003 single-sideband mixer, shared by the RX and TX path of the same index. */
class SCOPY_NEPONSET_EXPORT NeponsetLo : public NeponsetPathTool
{
	Q_OBJECT
public:
	NeponsetLo(NeponsetTopology *topology, IIOWidgetGroup *group, QWidget *parent = nullptr);

protected:
	QWidget *createPathPage(int index, QWidget *parent) override;

private:
	NeponsetTopology *m_topology = nullptr;
	NeponsetChipWidgets *m_chips = nullptr;
};

} // namespace scopy::neponset

#endif // NEPONSETPATHTOOL_H
