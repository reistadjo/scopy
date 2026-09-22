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

#ifndef NEPONSETTOPOLOGY_H
#define NEPONSETTOPOLOGY_H

#include "scopy-neponset_export.h"

#include <QString>
#include <QStringList>
#include <iio.h>

namespace scopy::neponset {

/** Number of independent RF paths on a Neponset module. */
static constexpr int PathCount = 4;

/**
 * One receive path: ADMV1420 downconverter, ADMV8809 low-band tunable filter and
 * ADMV8827 high-band tunable filter.
 *
 * Note the RX filter companion is an ADMV8809, not an ADMV8909 - the 8909 is on the
 * transmit side only. Both the device tree (admv8809_rx_N / admv8909_tx_N) and the
 * band mapping spreadsheet agree on this.
 */
struct RxPath
{
	int index = -1;
	iio_device *conv = nullptr;	// admv1420_rx_N
	iio_device *filtLow = nullptr;	// admv8809_rx_N
	iio_device *filtHigh = nullptr; // admv8827_rx_N

	bool isValid() const { return conv != nullptr; }
};

/**
 * One transmit path: ADMV1320 upconverter, ADMV8827 high-band tunable filter and
 * ADMV8909 wideband tunable filter.
 */
struct TxPath
{
	int index = -1;
	iio_device *conv = nullptr;	// admv1320_tx_N
	iio_device *filtHigh = nullptr; // admv8827_tx_N
	iio_device *filtWide = nullptr; // admv8909_tx_N

	bool isValid() const { return conv != nullptr; }
};

/** One LO path: HMC7003 single-sideband mixer. Shared by the RX and TX path of the same index. */
struct LoPath
{
	int index = -1;
	iio_device *mixer = nullptr; // hmc7003_lo_N

	bool isValid() const { return mixer != nullptr; }
};

/**
 * Discovers the Neponset device topology in an IIO context.
 *
 * Missing devices are tolerated: a partially populated module still yields a usable
 * plugin, with the absent paths reported through missingDevices().
 */
class SCOPY_NEPONSET_EXPORT NeponsetTopology
{
public:
	explicit NeponsetTopology(iio_context *ctx);

	/** True when the context holds at least one RX converter and one TX converter. */
	static bool isNeponsetContext(iio_context *ctx);

	iio_context *context() const { return m_ctx; }

	const RxPath &rx(int index) const;
	const TxPath &tx(int index) const;
	const LoPath &lo(int index) const;

	/** Board level devices. Either may be null. Their attributes are not documented, so
	 *  the UI enumerates whatever the driver exposes instead of hard-coding names. */
	iio_device *gpio() const { return m_gpio; }
	iio_device *status() const { return m_status; }

	/** Names of every expected device that was not found, for logging and diagnostics. */
	const QStringList &missingDevices() const { return m_missing; }

private:
	iio_device *findDevice(const QString &name);

	iio_context *m_ctx = nullptr;
	RxPath m_rx[PathCount];
	TxPath m_tx[PathCount];
	LoPath m_lo[PathCount];
	iio_device *m_gpio = nullptr;
	iio_device *m_status = nullptr;
	QStringList m_missing;
};

/**
 * Finds a channel by id and direction.
 *
 * Direction matters: libiio distinguishes altvoltage0 input from altvoltage0 output, and
 * the Neponset drivers mix the two on the same device. The flags below mirror the pyadi-iio
 * drivers exactly.
 */
SCOPY_NEPONSET_EXPORT iio_channel *findChannel(iio_device *dev, const char *id, bool output);

/** ADMV1420/ADMV1320 RF channel: altvoltage0, input. */
SCOPY_NEPONSET_EXPORT iio_channel *convRfChannel(iio_device *dev);
/** ADMV1420/ADMV1320 IF channel: altvoltage1, output. */
SCOPY_NEPONSET_EXPORT iio_channel *convIfChannel(iio_device *dev);
/** ADMV1420/ADMV1320 LO channel: altvoltage2, input. */
SCOPY_NEPONSET_EXPORT iio_channel *convLoChannel(iio_device *dev);
/** ADMV8827/ADMV8809/ADMV8909 RF channel: altvoltage0, input. */
SCOPY_NEPONSET_EXPORT iio_channel *filterRfChannel(iio_device *dev);
/** HMC7003 output channel: altvoltage0, output. */
SCOPY_NEPONSET_EXPORT iio_channel *mixerChannel(iio_device *dev);

/** Device name as libiio reports it, falling back to the device id. Never null. */
SCOPY_NEPONSET_EXPORT QString deviceName(iio_device *dev);

} // namespace scopy::neponset

#endif // NEPONSETTOPOLOGY_H
