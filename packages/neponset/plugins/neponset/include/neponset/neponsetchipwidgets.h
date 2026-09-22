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

#ifndef NEPONSETCHIPWIDGETS_H
#define NEPONSETCHIPWIDGETS_H

#include "scopy-neponset_export.h"

#include <QGridLayout>
#include <QObject>
#include <QString>
#include <QWidget>
#include <iio.h>

#include <iio-widgets/iiowidget.h>
#include <iio-widgets/iiowidgetbuilder.h>
#include <menucollapsesection.h>
#include <menusectionwidget.h>

namespace scopy {
class IIOWidgetGroup;
}

namespace scopy::neponset {

/**
 * Builds the per-chip control panels out of IIOWidgets.
 *
 * Same role as AD936xHelper in the ad936x package: one place that knows how each part's
 * attributes should be presented, so the RX, TX and LO tools only have to decide which chips
 * belong on which page.
 *
 * Every builder tolerates a null device and skips attributes the driver does not expose. That
 * is not defensive padding - the pyadi-iio drivers and the board's actual driver build differ
 * in places (ADMV8909 ps_en exists on the board but not in pyadi; the ADMV1320 IF band enum
 * does not match its own docstring), so a panel that insisted on its full attribute list would
 * fail to build rather than showing what is there.
 */
class SCOPY_NEPONSET_EXPORT NeponsetChipWidgets : public QObject
{
	Q_OBJECT
public:
	NeponsetChipWidgets(iio_context *ctx, IIOWidgetGroup *group, QObject *parent = nullptr);
	~NeponsetChipWidgets();

	/** ADMV1420 downconverter: RF, IF and LO channels, table controls and LUT editors. */
	QWidget *buildAdmv1420(iio_device *dev, QWidget *parent);
	/** ADMV1320 upconverter: RF, IF and LO channels, table controls and LUT editors. */
	QWidget *buildAdmv1320(iio_device *dev, QWidget *parent);
	/** ADMV8827 or ADMV8809 tunable band-select filter. They share a driver. */
	QWidget *buildTunableFilter(iio_device *dev, const QString &title, QWidget *parent);
	/** ADMV8909 wideband tunable filter with switch matrix and fast latch. */
	QWidget *buildAdmv8909(iio_device *dev, QWidget *parent);
	/** HMC7003 single-sideband LO mixer. */
	QWidget *buildHmc7003(iio_device *dev, QWidget *parent);

	/**
	 * Every attribute of a device, enumerated at runtime.
	 *
	 * Used for neponset_gpio and neponset_status, whose attributes are not described by any
	 * driver in pyadi-iio. Rather than guess at names for the mezzanine discretes
	 * (RX_ADDF/ADDG/LOAD, TX_ADDF/ADDG/LOAD, LO_FILTER_SEL) this renders whatever the driver
	 * actually publishes.
	 *
	 * TODO: replace with named, typed widgets - a 5 bit ADDF spinbox, a 6 bit ADDG spinbox,
	 * LOAD triggers and a 2 bit per-channel LO filter select - once the attribute list for
	 * neponset_gpio is known. The band presets already look up the LO filter select by
	 * pattern in NeponsetApplier::applyLoFilterSwitch.
	 */
	QWidget *buildGenericDevice(iio_device *dev, const QString &title, QWidget *parent);

	/** ADMV1420 temperature and power detector read-backs, polled. */
	QWidget *buildMonitors(iio_device *dev, const QString &title, QWidget *parent);

Q_SIGNALS:
	/** Ask every widget built by this helper to re-read its attribute. */
	void readRequested();

private:
	/** Collapsible section with a grid body. */
	MenuSectionCollapseWidget *makeSection(const QString &title, QWidget *parent, bool collapsed = false);

	// Each helper returns nullptr when the device, channel or attribute is absent, so
	// callers can hand the result straight to GridFiller::add.
	IIOWidget *deviceCombo(iio_device *dev, const char *attribute, const QString &title,
			       const QString &explicitOptions = QString(), const QString &info = QString());
	IIOWidget *deviceCheck(iio_device *dev, const char *attribute, const QString &title,
			       const QString &info = QString());
	IIOWidget *deviceRange(iio_device *dev, const char *attribute, const QString &title, int min, int max,
			       int step = 1, const QString &info = QString());

	IIOWidget *channelCombo(iio_channel *chn, const char *attribute, const QString &title,
				const QString &explicitOptions = QString(), const QString &info = QString());
	IIOWidget *channelCheck(iio_channel *chn, const char *attribute, const QString &title,
				const QString &info = QString());
	IIOWidget *channelRange(iio_channel *chn, const char *attribute, const QString &title, int min, int max,
				int step = 1, const QString &info = QString());

	/** Disabled widget polled on a timer that gives up after two consecutive failures. */
	IIOWidget *channelMonitor(iio_channel *chn, const char *attribute, const QString &title, int intervalMs,
				  QWidget *parent);

	/** Adds a LUT editor for `attribute` if the device exposes it. */
	QWidget *lutEditor(iio_device *dev, const char *attribute, const QString &title, QWidget *parent);

	iio_context *m_ctx = nullptr;
	IIOWidgetGroup *m_group = nullptr;
};

/**
 * Fills a grid left to right, skipping nulls so absent attributes leave no gaps.
 */
class SCOPY_NEPONSET_EXPORT GridFiller
{
public:
	GridFiller(QGridLayout *layout, int columns)
		: m_layout(layout)
		, m_columns(columns)
	{}

	void add(QWidget *widget);
	bool isEmpty() const { return m_index == 0; }

private:
	QGridLayout *m_layout = nullptr;
	int m_columns = 1;
	int m_index = 0;
};

} // namespace scopy::neponset

#endif // NEPONSETCHIPWIDGETS_H
