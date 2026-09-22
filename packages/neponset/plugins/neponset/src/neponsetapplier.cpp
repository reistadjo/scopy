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

#include "neponsetapplier.h"

#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(CAT_NEPONSET_APPLIER, "NeponsetApplier")

using namespace scopy::neponset;

namespace {

constexpr size_t AttrBufferSize = 1024;

/** Splits an *_available value on whitespace and commas. */
QStringList splitOptions(const QString &raw)
{
	static const QRegularExpression separator("[\\s,]+");
	QStringList options = raw.split(separator, Qt::SkipEmptyParts);
	for(QString &option : options) {
		option = option.trimmed();
	}
	return options;
}

/** Interprets an IIO boolean read-back. Returns false for anything unrecognised. */
bool parseBool(const QString &raw, bool *ok = nullptr)
{
	const QString value = raw.trimmed().toLower();
	if(value == "1" || value == "true" || value == "y" || value == "yes" || value == "on" ||
	   value == "enabled") {
		if(ok) {
			*ok = true;
		}
		return true;
	}
	if(value == "0" || value == "false" || value == "n" || value == "no" || value == "off" ||
	   value == "disabled") {
		if(ok) {
			*ok = true;
		}
		return false;
	}
	if(ok) {
		*ok = false;
	}
	return false;
}

/** True when a mode recipe value is a boolean rather than an enum such as "spi". */
bool looksBoolean(const QString &value)
{
	bool ok = false;
	parseBool(value, &ok);
	return ok;
}

/**
 * Compares a written value with its read-back.
 *
 * Some of these attributes are numeric even though they behave like enumerations - the
 * HMC7003 hardwaregain and phase are read as numbers by the driver, so a written "6" can
 * come back as "6.000000". Falling back to a numeric comparison keeps that from being
 * reported as a failed write.
 */
bool valuesMatch(const QString &written, const QString &readBack)
{
	if(written == readBack) {
		return true;
	}

	bool writtenOk = false;
	bool readBackOk = false;
	const double a = written.toDouble(&writtenOk);
	const double b = readBack.toDouble(&readBackOk);
	if(!writtenOk || !readBackOk) {
		return false;
	}
	return qFuzzyCompare(a + 1.0, b + 1.0);
}

QString levelTag(ApplyLevel level)
{
	switch(level) {
	case ApplyLevel::Ok:
		return QStringLiteral("ok   ");
	case ApplyLevel::Note:
		return QStringLiteral("note ");
	case ApplyLevel::Skipped:
		return QStringLiteral("skip ");
	case ApplyLevel::Failed:
		return QStringLiteral("FAIL ");
	}
	return QStringLiteral("?    ");
}

} // namespace

// --- ApplyEntry ---

QString ApplyEntry::toString() const
{
	QString line = levelTag(level);

	if(!device.isEmpty()) {
		line += device;
	}
	if(!attribute.isEmpty()) {
		line += "/" + attribute;
	}
	if(!requested.isEmpty()) {
		line += " = " + requested;
	}
	if(!actual.isEmpty() && actual != requested) {
		line += " (read back " + actual + ")";
	}
	if(!message.isEmpty()) {
		line += "  -- " + message;
	}
	return line;
}

// --- ApplyResult ---

void ApplyResult::append(const ApplyEntry &entry)
{
	entries.append(entry);
	switch(entry.level) {
	case ApplyLevel::Ok:
		written++;
		break;
	case ApplyLevel::Note:
		written++;
		notes++;
		break;
	case ApplyLevel::Skipped:
		skipped++;
		break;
	case ApplyLevel::Failed:
		failed++;
		break;
	}
}

void ApplyResult::merge(const ApplyResult &other)
{
	entries.append(other.entries);
	written += other.written;
	notes += other.notes;
	skipped += other.skipped;
	failed += other.failed;
}

QString ApplyResult::summary() const
{
	QStringList parts;
	parts << QString("%1 written").arg(written);
	if(notes) {
		parts << QString("%1 adjusted").arg(notes);
	}
	if(skipped) {
		parts << QString("%1 skipped").arg(skipped);
	}
	if(failed) {
		parts << QString("%1 failed").arg(failed);
	}
	return parts.join(", ");
}

QString ApplyResult::detail() const
{
	QStringList lines;
	for(const ApplyEntry &entry : entries) {
		lines << entry.toString();
	}
	return lines.join("\n");
}

// --- NeponsetApplier ---

NeponsetApplier::NeponsetApplier(NeponsetTopology *topology, QObject *parent)
	: QObject(parent)
	, m_topology(topology)
{}

NeponsetApplier::~NeponsetApplier() {}

void NeponsetApplier::noteMissingDevice(const QString &expectedName, const QString &role, ApplyResult &result)
{
	ApplyEntry entry;
	entry.level = ApplyLevel::Skipped;
	entry.device = expectedName;
	entry.message = QString("%1 not present in this context").arg(role);
	result.append(entry);
}

QStringList NeponsetApplier::deviceOptions(iio_device *dev, const char *attribute)
{
	if(!dev) {
		return {};
	}

	const QString availableName = QString(attribute) + "_available";
	if(!iio_device_find_attr(dev, availableName.toStdString().c_str())) {
		return {};
	}

	char buffer[AttrBufferSize] = {0};
	ssize_t ret = iio_device_attr_read(dev, availableName.toStdString().c_str(), buffer, sizeof(buffer));
	if(ret < 0) {
		return {};
	}
	return splitOptions(QString(buffer));
}

QStringList NeponsetApplier::channelOptions(iio_channel *chn, const char *attribute)
{
	if(!chn) {
		return {};
	}

	const QString availableName = QString(attribute) + "_available";
	if(!iio_channel_find_attr(chn, availableName.toStdString().c_str())) {
		return {};
	}

	char buffer[AttrBufferSize] = {0};
	ssize_t ret = iio_channel_attr_read(chn, availableName.toStdString().c_str(), buffer, sizeof(buffer));
	if(ret < 0) {
		return {};
	}
	return splitOptions(QString(buffer));
}

QString NeponsetApplier::resolveEnum(const QStringList &options, const QString &requested, QString &note)
{
	note.clear();

	if(options.isEmpty()) {
		// The device does not publish its choices, so there is nothing to check against.
		return requested;
	}

	if(options.contains(requested)) {
		return requested;
	}

	// Numeric options may be published in a different format than the spreadsheet uses
	// (for instance "6.000000" against "6"), so match on value and write back the exact
	// spelling the device offered.
	for(const QString &option : options) {
		if(valuesMatch(requested, option)) {
			return option;
		}
	}

	for(const QString &alias : enumAliases(requested)) {
		if(options.contains(alias)) {
			note = QString("spreadsheet asks for \"%1\", device offers \"%2\"; used the alias")
				       .arg(requested, alias);
			return alias;
		}
	}

	note = QString("\"%1\" is not offered by the device (available: %2)").arg(requested, options.join(" "));
	return QString();
}

void NeponsetApplier::writeDeviceEnum(iio_device *dev, const char *attribute, const QString &requested,
				      ApplyResult &result)
{
	ApplyEntry entry;
	entry.device = deviceName(dev);
	entry.attribute = attribute;
	entry.requested = requested;

	if(!dev) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "device not present";
		result.append(entry);
		return;
	}

	if(!iio_device_find_attr(dev, attribute)) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "attribute not exposed by this driver";
		result.append(entry);
		return;
	}

	QString note;
	const QString value = resolveEnum(deviceOptions(dev, attribute), requested, note);
	if(value.isEmpty()) {
		entry.level = ApplyLevel::Skipped;
		entry.message = note;
		result.append(entry);
		return;
	}

	ssize_t ret = iio_device_attr_write(dev, attribute, value.toStdString().c_str());
	if(ret < 0) {
		entry.level = ApplyLevel::Failed;
		entry.message = QString("write returned %1").arg(ret);
		result.append(entry);
		return;
	}

	char buffer[AttrBufferSize] = {0};
	if(iio_device_attr_read(dev, attribute, buffer, sizeof(buffer)) >= 0) {
		entry.actual = QString(buffer).trimmed();
	}

	if(!entry.actual.isEmpty() && !valuesMatch(value, entry.actual)) {
		entry.level = ApplyLevel::Failed;
		entry.message = QString("wrote \"%1\" but the device reports \"%2\"").arg(value, entry.actual);
	} else if(!note.isEmpty()) {
		entry.level = ApplyLevel::Note;
		entry.message = note;
	} else {
		entry.level = ApplyLevel::Ok;
	}
	result.append(entry);
}

void NeponsetApplier::writeDeviceInt(iio_device *dev, const char *attribute, int requested, ApplyResult &result)
{
	ApplyEntry entry;
	entry.device = deviceName(dev);
	entry.attribute = attribute;
	entry.requested = QString::number(requested);

	if(!dev) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "device not present";
		result.append(entry);
		return;
	}

	if(!iio_device_find_attr(dev, attribute)) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "attribute not exposed by this driver";
		result.append(entry);
		return;
	}

	ssize_t ret = iio_device_attr_write(dev, attribute, entry.requested.toStdString().c_str());
	if(ret < 0) {
		entry.level = ApplyLevel::Failed;
		entry.message = QString("write returned %1").arg(ret);
		result.append(entry);
		return;
	}

	// Read back rather than trusting a documented range. This is what catches a driver
	// clamping an out-of-range value, which is the failure mode the spreadsheet's
	// ADMV8909 HPF = 7 would hit against a 0-3 register.
	char buffer[AttrBufferSize] = {0};
	if(iio_device_attr_read(dev, attribute, buffer, sizeof(buffer)) >= 0) {
		entry.actual = QString(buffer).trimmed();
	}

	if(!entry.actual.isEmpty() && entry.actual.toLongLong() != requested) {
		entry.level = ApplyLevel::Failed;
		entry.message = QString("device clamped or rejected the value, it reports %1").arg(entry.actual);
	} else {
		entry.level = ApplyLevel::Ok;
	}
	result.append(entry);
}

void NeponsetApplier::writeDeviceBool(iio_device *dev, const char *attribute, bool requested, ApplyResult &result)
{
	ApplyEntry entry;
	entry.device = deviceName(dev);
	entry.attribute = attribute;
	entry.requested = requested ? "1" : "0";

	if(!dev) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "device not present";
		result.append(entry);
		return;
	}

	if(!iio_device_find_attr(dev, attribute)) {
		// Expected for admv8909 ps_en on drivers that predate it. Worth reporting, not
		// worth aborting a mode change over.
		entry.level = ApplyLevel::Skipped;
		entry.message = "attribute not exposed by this driver";
		result.append(entry);
		return;
	}

	// Neponset_Control.txt writes these as "true"/"false" while most IIO drivers parse
	// "1"/"0". Try the numeric form first and fall back, so the recipe works either way.
	const QStringList candidates = requested ? QStringList{"1", "true"} : QStringList{"0", "false"};

	ssize_t lastRet = 0;
	for(const QString &candidate : candidates) {
		lastRet = iio_device_attr_write(dev, attribute, candidate.toStdString().c_str());
		if(lastRet < 0) {
			continue;
		}

		char buffer[AttrBufferSize] = {0};
		if(iio_device_attr_read(dev, attribute, buffer, sizeof(buffer)) < 0) {
			// Write accepted but unreadable: take the write at its word.
			entry.level = ApplyLevel::Ok;
			result.append(entry);
			return;
		}

		entry.actual = QString(buffer).trimmed();
		bool parsed = false;
		const bool readBack = parseBool(entry.actual, &parsed);
		if(!parsed) {
			entry.level = ApplyLevel::Note;
			entry.message = QString("read back \"%1\", which is not a recognised boolean")
						.arg(entry.actual);
			result.append(entry);
			return;
		}

		if(readBack == requested) {
			if(candidate != candidates.first()) {
				entry.level = ApplyLevel::Note;
				entry.message = QString("driver wanted the \"%1\" spelling").arg(candidate);
			} else {
				entry.level = ApplyLevel::Ok;
			}
			result.append(entry);
			return;
		}
	}

	entry.level = ApplyLevel::Failed;
	entry.message = QString("no accepted spelling, last write returned %1").arg(lastRet);
	result.append(entry);
}

void NeponsetApplier::writeChannelEnum(iio_device *dev, iio_channel *chn, const char *attribute,
				       const QString &requested, ApplyResult &result)
{
	ApplyEntry entry;
	entry.device = deviceName(dev);
	entry.attribute = attribute;
	entry.requested = requested;

	if(!chn) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "channel not present";
		result.append(entry);
		return;
	}

	const char *channelId = iio_channel_get_id(chn);
	if(channelId) {
		entry.attribute = QString("%1/%2").arg(channelId, attribute);
	}

	if(!iio_channel_find_attr(chn, attribute)) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "attribute not exposed by this driver";
		result.append(entry);
		return;
	}

	QString note;
	const QString value = resolveEnum(channelOptions(chn, attribute), requested, note);
	if(value.isEmpty()) {
		entry.level = ApplyLevel::Skipped;
		entry.message = note;
		result.append(entry);
		return;
	}

	ssize_t ret = iio_channel_attr_write(chn, attribute, value.toStdString().c_str());
	if(ret < 0) {
		entry.level = ApplyLevel::Failed;
		entry.message = QString("write returned %1").arg(ret);
		result.append(entry);
		return;
	}

	char buffer[AttrBufferSize] = {0};
	if(iio_channel_attr_read(chn, attribute, buffer, sizeof(buffer)) >= 0) {
		entry.actual = QString(buffer).trimmed();
	}

	if(!entry.actual.isEmpty() && !valuesMatch(value, entry.actual)) {
		entry.level = ApplyLevel::Failed;
		entry.message = QString("wrote \"%1\" but the device reports \"%2\"").arg(value, entry.actual);
	} else if(!note.isEmpty()) {
		entry.level = ApplyLevel::Note;
		entry.message = note;
	} else {
		entry.level = ApplyLevel::Ok;
	}
	result.append(entry);
}

void NeponsetApplier::writeChannelInt(iio_device *dev, iio_channel *chn, const char *attribute, int requested,
				      ApplyResult &result)
{
	ApplyEntry entry;
	entry.device = deviceName(dev);
	entry.attribute = attribute;
	entry.requested = QString::number(requested);

	if(!chn) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "channel not present";
		result.append(entry);
		return;
	}

	const char *channelId = iio_channel_get_id(chn);
	if(channelId) {
		entry.attribute = QString("%1/%2").arg(channelId, attribute);
	}

	if(!iio_channel_find_attr(chn, attribute)) {
		entry.level = ApplyLevel::Skipped;
		entry.message = "attribute not exposed by this driver";
		result.append(entry);
		return;
	}

	ssize_t ret = iio_channel_attr_write(chn, attribute, entry.requested.toStdString().c_str());
	if(ret < 0) {
		entry.level = ApplyLevel::Failed;
		entry.message = QString("write returned %1").arg(ret);
		result.append(entry);
		return;
	}

	char buffer[AttrBufferSize] = {0};
	if(iio_channel_attr_read(chn, attribute, buffer, sizeof(buffer)) >= 0) {
		entry.actual = QString(buffer).trimmed();
	}

	if(!entry.actual.isEmpty() && entry.actual.toLongLong() != requested) {
		entry.level = ApplyLevel::Failed;
		entry.message = QString("device clamped or rejected the value, it reports %1").arg(entry.actual);
	} else {
		entry.level = ApplyLevel::Ok;
	}
	result.append(entry);
}

void NeponsetApplier::writeModeRecipe(iio_device *dev, const QVector<ModeWrite> &writes, ApplyResult &result)
{
	for(const ModeWrite &write : writes) {
		const QString value(write.value);
		if(looksBoolean(value)) {
			writeDeviceBool(dev, write.attribute, parseBool(value), result);
		} else {
			writeDeviceEnum(dev, write.attribute, value, result);
		}
	}
}

// --- mode ---

ApplyResult NeponsetApplier::applyMode(NeponsetMode mode, int pathIndex)
{
	ApplyResult result;

	if(!m_topology) {
		ApplyEntry entry;
		entry.level = ApplyLevel::Failed;
		entry.message = "no topology available";
		result.append(entry);
		return result;
	}

	if(pathIndex >= 0) {
		applyModeToPath(mode, pathIndex, result);
	} else {
		for(int i = 0; i < PathCount; ++i) {
			applyModeToPath(mode, i, result);
		}
	}

	qInfo(CAT_NEPONSET_APPLIER) << modeName(mode) << "mode:" << result.summary();
	Q_EMIT configurationChanged();
	return result;
}

QString NeponsetApplier::detectMode(int pathIndex) const
{
	if(!m_topology) {
		return QStringLiteral("unknown");
	}

	int bypassVotes = 0;
	int lutVotes = 0;

	// bypassWhenTrue says which mode a set bit indicates for that attribute.
	auto vote = [&](iio_device *dev, const char *attribute, bool bypassWhenTrue) {
		if(!dev || !iio_device_find_attr(dev, attribute)) {
			return;
		}

		char buffer[AttrBufferSize] = {0};
		if(iio_device_attr_read(dev, attribute, buffer, sizeof(buffer)) < 0) {
			return;
		}

		bool parsed = false;
		const bool value = parseBool(QString(buffer), &parsed);
		if(!parsed) {
			return;
		}

		if(value == bypassWhenTrue) {
			bypassVotes++;
		} else {
			lutVotes++;
		}
	};

	const RxPath &rx = m_topology->rx(pathIndex);
	const TxPath &tx = m_topology->tx(pathIndex);

	// On the converters, bypass_gain_table_en is set in bypass mode and the table enables are
	// set in LUT mode.
	for(iio_device *conv : {rx.conv, tx.conv}) {
		vote(conv, "bypass_gain_table_en", true);
		vote(conv, "gain_table_en", false);
		vote(conv, "filter_table_en", false);
	}

	// On the filters, lut_bypass is set in bypass mode.
	for(iio_device *filt : {rx.filtLow, rx.filtHigh, tx.filtHigh}) {
		vote(filt, "lut_bypass", true);
	}

	if(bypassVotes == 0 && lutVotes == 0) {
		return QStringLiteral("unknown");
	}
	if(lutVotes == 0) {
		return QStringLiteral("bypass");
	}
	if(bypassVotes == 0) {
		return QStringLiteral("lut");
	}
	return QStringLiteral("mixed");
}

void NeponsetApplier::applyModeToPath(NeponsetMode mode, int pathIndex, ApplyResult &result)
{
	const RxPath &rx = m_topology->rx(pathIndex);
	const TxPath &tx = m_topology->tx(pathIndex);

	if(rx.isValid()) {
		writeModeRecipe(rx.conv, converterModeWrites(mode), result);
	}
	if(rx.filtLow) {
		writeModeRecipe(rx.filtLow, tunableFilterModeWrites(mode), result);
	}
	if(rx.filtHigh) {
		writeModeRecipe(rx.filtHigh, tunableFilterModeWrites(mode), result);
	}

	if(tx.isValid()) {
		writeModeRecipe(tx.conv, converterModeWrites(mode), result);
	}
	if(tx.filtHigh) {
		writeModeRecipe(tx.filtHigh, tunableFilterModeWrites(mode), result);
	}
	if(tx.filtWide) {
		writeModeRecipe(tx.filtWide, wideFilterModeWrites(mode), result);
	}
}

// --- bands ---

ApplyResult NeponsetApplier::applyRxBand(int pathIndex, const RxBandConfig &band, bool deriveLoX3Filter)
{
	ApplyResult result;
	if(!m_topology) {
		return result;
	}

	applyRxChips(pathIndex, band, deriveLoX3Filter, result);
	applySharedLo(pathIndex, band.mixerFeedthru, band.mixerSideband, band.mixerDivider, band.mixerGain,
		      band.mixerPhase, band.loFilterSw, result);

	qInfo(CAT_NEPONSET_APPLIER) << "RX" << pathIndex << "band" << band.name << ":" << result.summary();
	Q_EMIT configurationChanged();
	return result;
}

ApplyResult NeponsetApplier::applyTxBand(int pathIndex, const TxBandConfig &band, bool deriveLoX3Filter)
{
	ApplyResult result;
	if(!m_topology) {
		return result;
	}

	applyTxChips(pathIndex, band, deriveLoX3Filter, result);
	applySharedLo(pathIndex, band.mixerFeedthru, band.mixerSideband, band.mixerDivider, band.mixerGain,
		      band.mixerPhase, band.loFilterSw, result);

	qInfo(CAT_NEPONSET_APPLIER) << "TX" << pathIndex << "band" << band.name << ":" << result.summary();
	Q_EMIT configurationChanged();
	return result;
}

ApplyResult NeponsetApplier::applyChannel(int pathIndex, const RxBandConfig *rxBand, const TxBandConfig *txBand,
					  bool deriveLoX3Filter)
{
	ApplyResult result;
	if(!m_topology) {
		return result;
	}

	if(rxBand) {
		applyRxChips(pathIndex, *rxBand, deriveLoX3Filter, result);
	}
	if(txBand) {
		applyTxChips(pathIndex, *txBand, deriveLoX3Filter, result);
	}

	const bool loConflict = rxBand && txBand && !qFuzzyCompare(rxBand->loGHz, txBand->loGHz);
	if(loConflict) {
		ApplyEntry entry;
		entry.level = ApplyLevel::Skipped;
		entry.device = deviceName(m_topology->lo(pathIndex).mixer);
		entry.message = QString("RX band %1 needs a %2 GHz LO but TX band %3 needs %4 GHz; RX and TX "
					"share this LO, so the HMC7003 and LO filter switch were left "
					"unchanged. Pick a band pair with a common LO.")
					.arg(rxBand->name)
					.arg(rxBand->loGHz)
					.arg(txBand->name)
					.arg(txBand->loGHz);
		result.append(entry);
	} else if(rxBand) {
		applySharedLo(pathIndex, rxBand->mixerFeedthru, rxBand->mixerSideband, rxBand->mixerDivider,
			      rxBand->mixerGain, rxBand->mixerPhase, rxBand->loFilterSw, result);
	} else if(txBand) {
		applySharedLo(pathIndex, txBand->mixerFeedthru, txBand->mixerSideband, txBand->mixerDivider,
			      txBand->mixerGain, txBand->mixerPhase, txBand->loFilterSw, result);
	}

	qInfo(CAT_NEPONSET_APPLIER) << "Channel" << pathIndex << ":" << result.summary();
	Q_EMIT configurationChanged();
	return result;
}

void NeponsetApplier::applyRxChips(int pathIndex, const RxBandConfig &band, bool deriveLoX3Filter,
				   ApplyResult &result)
{
	const RxPath &rx = m_topology->rx(pathIndex);

	if(!rx.isValid()) {
		noteMissingDevice(QString("admv1420_rx_%1").arg(pathIndex), "RX converter", result);
	} else {
		// ADMV1420
		writeChannelEnum(rx.conv, convRfChannel(rx.conv), "band", band.convRfBand, result);
		writeChannelEnum(rx.conv, convIfChannel(rx.conv), "band", band.convIfBand, result);

		// GPO_G carries the path switch (SW_CTRL_IN / SW_CTRL_OUT), which matters in
		// bypass mode too. Write both shadow registers where they exist so the value
		// lands whichever one the driver consults; there is no bypass_gpo_f.
		writeDeviceInt(rx.conv, "direct_gpo_g", band.gpoG, result);
		if(iio_device_find_attr(rx.conv, "bypass_gpo_g")) {
			writeDeviceInt(rx.conv, "bypass_gpo_g", band.gpoG, result);
		}

		if(deriveLoX3Filter) {
			ApplyResult derived;
			writeChannelEnum(rx.conv, convLoChannel(rx.conv), "x3_filter",
					 rxConvLoX3Filter(band.loGHz), derived);
			for(ApplyEntry entry : derived.entries) {
				entry.message = entry.message.isEmpty()
							? QString("derived from the %1 GHz LO, not from the "
								  "spreadsheet")
								  .arg(band.loGHz)
							: entry.message +
								" (derived from the LO, not from the spreadsheet)";
				if(entry.level == ApplyLevel::Ok) {
					entry.level = ApplyLevel::Note;
				}
				result.append(entry);
			}
		}
	}

	// ADMV8827, high band tunable filter
	if(rx.filtHigh) {
		iio_channel *chn = filterRfChannel(rx.filtHigh);
		writeChannelEnum(rx.filtHigh, chn, "band", band.filtHighBand, result);
		writeChannelInt(rx.filtHigh, chn, "direct_hpf_val", band.filtHighHpf, result);
		writeChannelInt(rx.filtHigh, chn, "direct_lpf_val", band.filtHighLpf, result);
	} else {
		noteMissingDevice(QString("admv8827_rx_%1").arg(pathIndex), "high band filter", result);
	}

	// ADMV8809, low band tunable filter
	if(rx.filtLow) {
		iio_channel *chn = filterRfChannel(rx.filtLow);
		writeChannelEnum(rx.filtLow, chn, "band", band.filtLowBand, result);
		writeChannelInt(rx.filtLow, chn, "direct_hpf_val", band.filtLowHpf, result);
		writeChannelInt(rx.filtLow, chn, "direct_lpf_val", band.filtLowLpf, result);
	} else {
		noteMissingDevice(QString("admv8809_rx_%1").arg(pathIndex), "low band filter", result);
	}
}

void NeponsetApplier::applyTxChips(int pathIndex, const TxBandConfig &band, bool deriveLoX3Filter,
				   ApplyResult &result)
{
	const TxPath &tx = m_topology->tx(pathIndex);

	if(!tx.isValid()) {
		noteMissingDevice(QString("admv1320_tx_%1").arg(pathIndex), "TX converter", result);
	} else {
		// ADMV1320
		writeChannelEnum(tx.conv, convRfChannel(tx.conv), "band", band.convRfBand, result);
		writeChannelEnum(tx.conv, convIfChannel(tx.conv), "band", band.convIfBand, result);

		// GPO_F bit 8 and GPO_G bit 0 are SW_SEL_0/SW_SEL_1, the filter path select.
		writeDeviceInt(tx.conv, "direct_gpo_f", band.gpoF, result);
		writeDeviceInt(tx.conv, "direct_gpo_g", band.gpoG, result);
		if(iio_device_find_attr(tx.conv, "bypass_gpo_g")) {
			writeDeviceInt(tx.conv, "bypass_gpo_g", band.gpoG, result);
		}

		if(deriveLoX3Filter) {
			ApplyResult derived;
			writeChannelEnum(tx.conv, convLoChannel(tx.conv), "x3_filter",
					 txConvLoX3Filter(band.loGHz), derived);
			for(ApplyEntry entry : derived.entries) {
				entry.message = entry.message.isEmpty()
							? QString("derived from the %1 GHz LO, not from the "
								  "spreadsheet")
								  .arg(band.loGHz)
							: entry.message +
								" (derived from the LO, not from the spreadsheet)";
				if(entry.level == ApplyLevel::Ok) {
					entry.level = ApplyLevel::Note;
				}
				result.append(entry);
			}
		}
	}

	// ADMV8827, high band tunable filter
	if(tx.filtHigh) {
		iio_channel *chn = filterRfChannel(tx.filtHigh);
		writeChannelEnum(tx.filtHigh, chn, "band", band.filtHighBand, result);
		writeChannelInt(tx.filtHigh, chn, "direct_hpf_val", band.filtHighHpf, result);
		writeChannelInt(tx.filtHigh, chn, "direct_lpf_val", band.filtHighLpf, result);
	} else {
		noteMissingDevice(QString("admv8827_tx_%1").arg(pathIndex), "high band filter", result);
	}

	// ADMV8909, wideband tunable filter with its own input/output switch
	if(tx.filtWide) {
		iio_channel *chn = filterRfChannel(tx.filtWide);
		writeChannelInt(tx.filtWide, chn, "direct_sw_in_val", band.filtWideSwIn, result);
		writeChannelInt(tx.filtWide, chn, "direct_sw_out_val", band.filtWideSwOut, result);
		writeChannelInt(tx.filtWide, chn, "direct_lpf_val", band.filtWideLpf, result);
		writeChannelInt(tx.filtWide, chn, "direct_hpf_val", band.filtWideHpf, result);
	} else {
		noteMissingDevice(QString("admv8909_tx_%1").arg(pathIndex), "wideband filter", result);
	}
}

void NeponsetApplier::applySharedLo(int pathIndex, const char *feedthru, const char *sideband, const char *divider,
				    const char *gain, const char *phase, int loFilterSw, ApplyResult &result)
{
	const LoPath &lo = m_topology->lo(pathIndex);

	if(!lo.isValid()) {
		noteMissingDevice(QString("hmc7003_lo_%1").arg(pathIndex), "LO mixer", result);
	} else {
		iio_channel *chn = mixerChannel(lo.mixer);
		// The spreadsheet's "Through" feedthru is the HMC7003 LO pass-through mode,
		// which the driver spells "lo"; the band tables already carry the driver
		// spelling.
		writeChannelEnum(lo.mixer, chn, "feedthru", feedthru, result);
		writeChannelEnum(lo.mixer, chn, "sideband", sideband, result);
		writeChannelEnum(lo.mixer, chn, "divider", divider, result);
		writeChannelEnum(lo.mixer, chn, "hardwaregain", gain, result);
		writeChannelEnum(lo.mixer, chn, "phase", phase, result);
	}

	applyLoFilterSwitch(pathIndex, loFilterSw, result);
}

void NeponsetApplier::applyLoFilterSwitch(int pathIndex, int value, ApplyResult &result)
{
	iio_device *gpio = m_topology->gpio();

	ApplyEntry entry;
	entry.device = gpio ? deviceName(gpio) : QStringLiteral("neponset_gpio");
	entry.requested = QString::number(value);

	if(!gpio) {
		entry.level = ApplyLevel::Skipped;
		entry.attribute = "lo_filter_sel";
		entry.message = "neponset_gpio not present, LO filter switch left unchanged";
		result.append(entry);
		return;
	}

	// The mezzanine discrete controls are not covered by any pyadi-iio driver, so the
	// attribute name is discovered rather than assumed. A per-path name wins over a
	// shared one.
	const int attrCount = iio_device_get_attrs_count(gpio);
	QString indexed;
	QString shared;
	for(int i = 0; i < attrCount; ++i) {
		const char *raw = iio_device_get_attr(gpio, i);
		if(!raw) {
			continue;
		}
		const QString name(raw);
		const QString lower = name.toLower();
		if(!lower.contains("lo") || !lower.contains("filter")) {
			continue;
		}
		if(!lower.contains("sel") && !lower.contains("sw")) {
			continue;
		}

		// Only treat a trailing digit as a path index when it is the whole suffix, so
		// "lo_filter_sel_31" is not mistaken for path 1.
		static const QRegularExpression trailingIndex("(?:^|[^0-9])([0-9]+)$");
		const QRegularExpressionMatch match = trailingIndex.match(lower);
		if(match.hasMatch()) {
			if(match.captured(1).toInt() == pathIndex) {
				indexed = name;
				break;
			}
			// A different path's attribute: never a candidate for this path.
			continue;
		}
		if(shared.isEmpty()) {
			shared = name;
		}
	}

	const QString attribute = !indexed.isEmpty() ? indexed : shared;
	if(attribute.isEmpty()) {
		entry.level = ApplyLevel::Skipped;
		entry.attribute = "lo_filter_sel";
		entry.message = "no LO filter select attribute found on neponset_gpio";
		result.append(entry);
		return;
	}

	writeDeviceInt(gpio, attribute.toStdString().c_str(), value, result);
}

#include "moc_neponsetapplier.cpp"
