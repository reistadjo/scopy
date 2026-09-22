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

#include "neponsettopology.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(CAT_NEPONSET_TOPOLOGY, "NeponsetTopology")

using namespace scopy::neponset;

namespace {

const RxPath EmptyRx = {};
const TxPath EmptyTx = {};
const LoPath EmptyLo = {};

} // namespace

NeponsetTopology::NeponsetTopology(iio_context *ctx)
	: m_ctx(ctx)
{
	if(!m_ctx) {
		qWarning(CAT_NEPONSET_TOPOLOGY) << "Null context, nothing to discover";
		return;
	}

	for(int i = 0; i < PathCount; ++i) {
		m_rx[i].index = i;
		m_rx[i].conv = findDevice(QString("admv1420_rx_%1").arg(i));
		m_rx[i].filtLow = findDevice(QString("admv8809_rx_%1").arg(i));
		m_rx[i].filtHigh = findDevice(QString("admv8827_rx_%1").arg(i));

		m_tx[i].index = i;
		m_tx[i].conv = findDevice(QString("admv1320_tx_%1").arg(i));
		m_tx[i].filtHigh = findDevice(QString("admv8827_tx_%1").arg(i));
		m_tx[i].filtWide = findDevice(QString("admv8909_tx_%1").arg(i));

		m_lo[i].index = i;
		m_lo[i].mixer = findDevice(QString("hmc7003_lo_%1").arg(i));
	}

	m_gpio = findDevice("neponset_gpio");
	m_status = findDevice("neponset_status");

	if(!m_missing.isEmpty()) {
		qInfo(CAT_NEPONSET_TOPOLOGY) << "Devices not present in context:" << m_missing.join(", ");
	}
}

bool NeponsetTopology::isNeponsetContext(iio_context *ctx)
{
	if(!ctx) {
		return false;
	}

	// Require both a receive and a transmit converter so this plugin does not claim a
	// context that merely happens to carry a single ADMV part on an eval board.
	return iio_context_find_device(ctx, "admv1420_rx_0") != nullptr &&
		iio_context_find_device(ctx, "admv1320_tx_0") != nullptr;
}

iio_device *NeponsetTopology::findDevice(const QString &name)
{
	iio_device *dev = iio_context_find_device(m_ctx, name.toStdString().c_str());
	if(!dev) {
		m_missing.append(name);
	}
	return dev;
}

const RxPath &NeponsetTopology::rx(int index) const
{
	if(index < 0 || index >= PathCount) {
		return EmptyRx;
	}
	return m_rx[index];
}

const TxPath &NeponsetTopology::tx(int index) const
{
	if(index < 0 || index >= PathCount) {
		return EmptyTx;
	}
	return m_tx[index];
}

const LoPath &NeponsetTopology::lo(int index) const
{
	if(index < 0 || index >= PathCount) {
		return EmptyLo;
	}
	return m_lo[index];
}

iio_channel *scopy::neponset::findChannel(iio_device *dev, const char *id, bool output)
{
	if(!dev) {
		return nullptr;
	}
	return iio_device_find_channel(dev, id, output);
}

iio_channel *scopy::neponset::convRfChannel(iio_device *dev) { return findChannel(dev, "altvoltage0", false); }

iio_channel *scopy::neponset::convIfChannel(iio_device *dev) { return findChannel(dev, "altvoltage1", true); }

iio_channel *scopy::neponset::convLoChannel(iio_device *dev) { return findChannel(dev, "altvoltage2", false); }

iio_channel *scopy::neponset::filterRfChannel(iio_device *dev) { return findChannel(dev, "altvoltage0", false); }

iio_channel *scopy::neponset::mixerChannel(iio_device *dev) { return findChannel(dev, "altvoltage0", true); }

QString scopy::neponset::deviceName(iio_device *dev)
{
	if(!dev) {
		return QString();
	}
	const char *name = iio_device_get_name(dev);
	if(!name) {
		name = iio_device_get_id(dev);
	}
	return name ? QString(name) : QString();
}
