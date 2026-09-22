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

#ifndef NEPONSETAPPLIER_H
#define NEPONSETAPPLIER_H

#include "scopy-neponset_export.h"
#include "neponsetbands.h"
#include "neponsettopology.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <iio.h>

namespace scopy::neponset {

enum class ApplyLevel
{
	Ok,	 // written and verified
	Note,	 // written, but not exactly as the spreadsheet asked
	Skipped, // not written, and the reason is known
	Failed	 // the write or the read-back failed
};

/** One line of the apply log. */
struct SCOPY_NEPONSET_EXPORT ApplyEntry
{
	ApplyLevel level = ApplyLevel::Ok;
	QString device;
	QString attribute;
	QString requested;
	QString actual;
	QString message;

	QString toString() const;
};

/**
 * Outcome of one mode change or band application.
 *
 * Deviations are first class here rather than an afterthought. The spreadsheet and the
 * pyadi-iio docstrings are both secondary sources that disagree with each other in at least
 * three places (ADMV8809 "through" band, ADMV1320 IF band strings, ADMV8909 HPF range), so a
 * preset that quietly wrote whatever the driver accepted would be worse than useless. Every
 * value is validated against the device before writing and read back afterwards.
 */
struct SCOPY_NEPONSET_EXPORT ApplyResult
{
	QVector<ApplyEntry> entries;

	int written = 0;
	int notes = 0;
	int skipped = 0;
	int failed = 0;

	bool isClean() const { return failed == 0 && skipped == 0 && notes == 0; }
	bool hasProblems() const { return failed > 0 || skipped > 0; }

	/** One line for the status bar, e.g. "22 written, 1 skipped". */
	QString summary() const;
	/** Full multi-line log for the in-tool view. */
	QString detail() const;

	void append(const ApplyEntry &entry);
	void merge(const ApplyResult &other);
};

/**
 * Applies operating modes and band presets across the Neponset device set.
 *
 * Writes go straight to libiio rather than through the IIOWidgets in the RX/TX/LO tools:
 * doing it the other way round would make correctness depend on the order the tools happen
 * to be constructed in. Once a batch completes, configurationChanged() is emitted so the
 * tools re-read and the UI catches up.
 */
class SCOPY_NEPONSET_EXPORT NeponsetApplier : public QObject
{
	Q_OBJECT
public:
	explicit NeponsetApplier(NeponsetTopology *topology, QObject *parent = nullptr);
	~NeponsetApplier();

	/** Bypass or LUT mode. pathIndex < 0 applies to every discovered path. */
	ApplyResult applyMode(NeponsetMode mode, int pathIndex = -1);

	/**
	 * Reads back which mode a path is actually in.
	 *
	 * There is no single mode attribute - the mode is a pattern across the table enables on
	 * both converters and the lut_bypass bit on all three filters - so this polls those bits
	 * and reports the consensus: "bypass", "lut", "mixed" when they disagree (which is what a
	 * half-applied mode change looks like), or "unknown" when none could be read.
	 */
	QString detectMode(int pathIndex) const;

	/** RX band preset for one path. Also writes the shared HMC7003 and LO filter switch. */
	ApplyResult applyRxBand(int pathIndex, const RxBandConfig &band, bool deriveLoX3Filter);

	/** TX band preset for one path. Also writes the shared HMC7003 and LO filter switch. */
	ApplyResult applyTxBand(int pathIndex, const TxBandConfig &band, bool deriveLoX3Filter);

	/**
	 * Applies an RX and a TX band to the same path.
	 *
	 * RX and TX on a path share an LO (Neponset_Control.txt), so their bands have to move
	 * together. When the two bands imply different LO frequencies the converter and filter
	 * settings are still applied, but the shared HMC7003 and LO filter switch are left
	 * alone and the conflict is recorded - picking one of the two silently would leave the
	 * other path mistuned with nothing to show for it.
	 *
	 * Either band may be null to leave that direction untouched.
	 */
	ApplyResult applyChannel(int pathIndex, const RxBandConfig *rxBand, const TxBandConfig *txBand,
				 bool deriveLoX3Filter);

Q_SIGNALS:
	/** Emitted after any successful batch so the tools can re-read every widget. */
	void configurationChanged();

private:
	void applyRxChips(int pathIndex, const RxBandConfig &band, bool deriveLoX3Filter, ApplyResult &result);
	void applyTxChips(int pathIndex, const TxBandConfig &band, bool deriveLoX3Filter, ApplyResult &result);
	void applySharedLo(int pathIndex, const char *feedthru, const char *sideband, const char *divider,
			   const char *gain, const char *phase, int loFilterSw, ApplyResult &result);
	void applyLoFilterSwitch(int pathIndex, int value, ApplyResult &result);
	void applyModeToPath(NeponsetMode mode, int pathIndex, ApplyResult &result);

	/** Records a named skip for a device the context does not contain. */
	static void noteMissingDevice(const QString &expectedName, const QString &role, ApplyResult &result);

	// --- write primitives, each appends exactly one entry to result ---

	void writeDeviceEnum(iio_device *dev, const char *attribute, const QString &requested, ApplyResult &result);
	void writeDeviceInt(iio_device *dev, const char *attribute, int requested, ApplyResult &result);
	void writeDeviceBool(iio_device *dev, const char *attribute, bool requested, ApplyResult &result);
	void writeChannelEnum(iio_device *dev, iio_channel *chn, const char *attribute, const QString &requested,
			      ApplyResult &result);
	void writeChannelInt(iio_device *dev, iio_channel *chn, const char *attribute, int requested,
			     ApplyResult &result);

	/** Writes the recipe, dispatching boolean-looking values to writeDeviceBool. */
	void writeModeRecipe(iio_device *dev, const QVector<ModeWrite> &writes, ApplyResult &result);

	/** Contents of <attribute>_available, whitespace and comma separated. Empty when absent. */
	QStringList deviceOptions(iio_device *dev, const char *attribute);
	QStringList channelOptions(iio_channel *chn, const char *attribute);

	/**
	 * Picks the value to actually write.
	 *
	 * Returns the requested value when the device lists it, a known alias when it lists
	 * that instead, or an empty string when neither is available. An empty options list
	 * means the device does not publish its choices, in which case the requested value is
	 * passed through unchecked.
	 */
	static QString resolveEnum(const QStringList &options, const QString &requested, QString &note);

	NeponsetTopology *m_topology = nullptr;
};

} // namespace scopy::neponset

#endif // NEPONSETAPPLIER_H
