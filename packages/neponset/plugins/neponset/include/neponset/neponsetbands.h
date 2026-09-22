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

#ifndef NEPONSETBANDS_H
#define NEPONSETBANDS_H

#include "scopy-neponset_export.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace scopy::neponset {

/**
 * The two high level operating modes described in Neponset_Control.txt.
 *
 * Bypass drives every chip over SPI and gives the most direct control, so it is what the
 * band presets use. Lut hands filter and gain selection to the on-chip lookup tables, which
 * are addressed in parallel from the mezzanine connector for fast hopping.
 */
enum class NeponsetMode
{
	Bypass,
	Lut
};

SCOPY_NEPONSET_EXPORT QString modeName(NeponsetMode mode);

/** A single attribute write in a mode recipe. */
struct ModeWrite
{
	const char *attribute;
	const char *value;
};

/** ADMV1420 / ADMV1320 table enable bits. */
SCOPY_NEPONSET_EXPORT QVector<ModeWrite> converterModeWrites(NeponsetMode mode);
/** ADMV8809 / ADMV8827 LUT access controls. */
SCOPY_NEPONSET_EXPORT QVector<ModeWrite> tunableFilterModeWrites(NeponsetMode mode);
/** ADMV8909 parallel select enable. Absent on drivers that predate the attribute. */
SCOPY_NEPONSET_EXPORT QVector<ModeWrite> wideFilterModeWrites(NeponsetMode mode);

/**
 * One row of the "RX Band Mapping" sheet of Neponest_Band_Mapping.xlsx.
 *
 * Every field is transcribed verbatim from the sheet. Values are validated against the
 * device's own *_available list before being written, so a sheet value the driver does not
 * recognise is reported rather than silently mangled.
 */
struct RxBandConfig
{
	const char *name;
	double startGHz;
	double endGHz;
	double loGHz;

	const char *convRfBand; // ADMV1420 RF band
	const char *convIfBand; // ADMV1420 IF band
	int gpoG;		// ADMV1420 GPO_G, selects the amp or filter path

	const char *filtHighBand; // ADMV8827 band
	int filtHighHpf;
	int filtHighLpf;

	const char *filtLowBand; // ADMV8809 band
	int filtLowHpf;
	int filtLowLpf;

	const char *mixerFeedthru; // HMC7003 feedthru
	const char *mixerSideband; // HMC7003 sideband
	const char *mixerDivider;  // HMC7003 divider
	const char *mixerGain;	   // HMC7003 hardwaregain, dB
	const char *mixerPhase;	   // HMC7003 phase, degrees

	int loFilterSw; // 2 bit LO filter switch select, mezzanine controlled
};

/** One row of the "TX Band Mapping" sheet of Neponest_Band_Mapping.xlsx. */
struct TxBandConfig
{
	const char *name;
	double startGHz;
	double endGHz;
	double loGHz;

	const char *convRfBand; // ADMV1320 RF band
	const char *convIfBand; // ADMV1320 IF band
	int gpoF;		// ADMV1320 GPO_F
	int gpoG;		// ADMV1320 GPO_G

	const char *filtHighBand; // ADMV8827 band
	int filtHighHpf;
	int filtHighLpf;

	int filtWideSwIn; // ADMV8909 switch input
	int filtWideSwOut;
	int filtWideLpf;
	int filtWideHpf;

	const char *mixerFeedthru;
	const char *mixerSideband;
	const char *mixerDivider;
	const char *mixerGain;
	const char *mixerPhase;

	int loFilterSw;
};

SCOPY_NEPONSET_EXPORT const QVector<RxBandConfig> &rxBands();
SCOPY_NEPONSET_EXPORT const QVector<TxBandConfig> &txBands();

SCOPY_NEPONSET_EXPORT const RxBandConfig *rxBandByName(const QString &name);
SCOPY_NEPONSET_EXPORT const TxBandConfig *txBandByName(const QString &name);

/** Combo label carrying both the sheet's band name and its frequency span, e.g. "0C-1  (5 - 7 GHz)".
 *  The span is included because Neponset_Control.txt numbers the 0C sub-bands one higher than the
 *  spreadsheet does, and the frequency range is unambiguous either way. */
SCOPY_NEPONSET_EXPORT QString bandLabel(const char *name, double startGHz, double endGHz);

/**
 * Converter LO x3 filter derived from the band's LO frequency.
 *
 * This is NOT in the spreadsheet - it is inferred from the LO column and the filter bands the
 * drivers expose. It is only applied when the user explicitly opts in, and is tagged as derived
 * in the apply log. The two converters expose different enums for the same physical bands.
 */
SCOPY_NEPONSET_EXPORT QString rxConvLoX3Filter(double loGHz);
SCOPY_NEPONSET_EXPORT QString txConvLoX3Filter(double loGHz);

/**
 * Alternative spellings to try when a spreadsheet enum value is missing from a device's
 * *_available list.
 *
 * The known case is the ADMV8809: the RX sheet asks for "through", but the 8809 driver
 * documents "bypass" as its pass-through band while only the ADMV8827 has a distinct
 * "through". Returns an empty list when no alias is known.
 */
SCOPY_NEPONSET_EXPORT QStringList enumAliases(const QString &value);

} // namespace scopy::neponset

#endif // NEPONSETBANDS_H
