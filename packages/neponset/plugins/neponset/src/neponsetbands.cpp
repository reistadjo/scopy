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

#include "neponsetbands.h"

using namespace scopy::neponset;

/*
 * GPO bit map, from the "RX GPO Mapping" and "TX GPO Mapping" sheets of
 * Neponest_Band_Mapping.xlsx. GPO_F is the low 9 bits, GPO_G the high 7, matching
 * direct_gpo_f (0-511) and direct_gpo_g (0-127) in the converter drivers.
 *
 *   bit | RX (ADMV1420)   | TX (ADMV1320)
 *   ----+-----------------+---------------
 *   F 0 | 8809_LUT_0      | 8909_LPF_0
 *   F 1 | 8809_LUT_1      | 8909_LPF_1
 *   F 2 | 8809_LUT_2      | 8909_LPF_2
 *   F 3 | 8809_LUT_3      | 8909_HPF_0
 *   F 4 | 8809_LUT_4      | 8909_HPF_1
 *   F 5 | 8827_LUT_0      | SW_IN_0
 *   F 6 | 8827_LUT_1      | SW_IN_1
 *   F 7 | 8827_LUT_2      | SW_OUT
 *   F 8 | 8827_LUT_3      | SW_SEL_0
 *   G 0 | 8827_LUT_4      | SW_SEL_1
 *   G 1 | SW_CTRL_IN      | 8827_LUT_0
 *   G 2 | -               | 8827_LUT_1
 *   G 3 | SW_CTRL_OUT     | 8827_LUT_2
 *   G 4 | -               | 8827_LUT_3
 *   G 5 | -               | 8827_LUT_4
 *
 * The LUT address bits only matter in LUT mode, but the switch bits matter in both modes,
 * which is why bypass mode band presets still write GPO. On RX that reduces to the two
 * values the sheet uses: GPO_G = 2 (SW_CTRL_IN) routes through the ADMV8809 filter, used by
 * every band up to 7 GHz, and GPO_G = 8 (SW_CTRL_OUT) takes the amplifier path above that.
 * On TX, SW_SEL_0 and SW_SEL_1 choose which filter sits in the path.
 *
 * Sheet note carried over verbatim, because it will matter on the next silicon:
 *   "NOTE: THIS MAPPING IS FOR CURRENT REV ADMV8909
 *    SW_IN_0/1 AND SW_OUT ARE SWAPPED AROUND FROM CURRENT DS, NEW IC REV WILL MATCH DS"
 */

namespace {

// clang-format off

/*
 * "RX Band Mapping" sheet. Row 0B is marked N/A throughout on the RX sheet and is omitted.
 *
 * name  start end   lo    convRf         convIf        gpoG  filtHigh (8827)    hpf lpf  filtLow (8809)  hpf lpf  feedthru sideband div gain phase  loSw
 */
const QVector<RxBandConfig> RxBandTable = {
	{"0A",    0.1,  3.0, 15.0, "100MHz_2GHz", "1GHz_5GHz",  2, "through",          0, 7, "through",      0, 7, "mixer", "LSB", "4", "6", "0", 0},
	{"0C-0",  3.0,  5.0, 15.0, "3GHz_13GHz",  "3GHz_13GHz", 2, "through",          0, 7, "2p2GHz_5GHz",  5, 6, "mixer", "LSB", "4", "6", "0", 0},
	{"0C-1",  5.0,  7.0, 15.0, "3GHz_13GHz",  "3GHz_13GHz", 2, "through",          0, 7, "4p5GHz_9GHz",  3, 2, "mixer", "LSB", "4", "6", "0", 0},
	{"0C-2",  7.0,  8.0, 15.0, "3GHz_13GHz",  "3GHz_13GHz", 8, "through",          0, 7, "4p5GHz_9GHz",  7, 4, "mixer", "LSB", "4", "6", "0", 0},
	{"1",     8.0, 12.0, 15.0, "6GHz_20GHz",  "3GHz_13GHz", 8, "5p7GHz_13GHz",     3, 7, "through",      0, 0, "mixer", "LSB", "4", "6", "0", 0},
	{"2",    12.0, 16.5, 20.0, "6GHz_20GHz",  "3GHz_13GHz", 8, "10p2GHz_19p2GHz",  1, 6, "through",      0, 0, "lo",    "LSB", "4", "6", "0", 1},
	{"3",    16.5, 20.0, 25.0, "6GHz_20GHz",  "3GHz_13GHz", 8, "16p5GHz_27p5GHz",  0, 2, "through",      0, 0, "mixer", "USB", "4", "6", "0", 3},
};

/*
 * "TX Band Mapping" sheet.
 *
 * name  start end   lo    convRf        convIf        gpoF gpoG  filtHigh (8827)    hpf lpf  8909 in out lpf hpf  feedthru sideband div gain phase  loSw
 */
const QVector<TxBandConfig> TxBandTable = {
	{"0A",    0.1,  1.0, 15.0, "0_2GHz",     "1GHz_5GHz",  256, 0, "through",          0, 7, 0, 0, 0, 0, "mixer", "LSB", "4", "6", "0", 0},
	{"0B",    1.0,  4.0, 15.0, "1GHz_5GHz",  "1GHz_5GHz",  256, 0, "through",          0, 7, 0, 0, 0, 0, "mixer", "LSB", "4", "6", "0", 0},
	{"0C",    4.0,  8.5, 15.0, "3GHz_10GHz", "3GHz_13GHz", 256, 0, "through",          0, 7, 2, 1, 6, 1, "mixer", "LSB", "4", "6", "0", 0},
	{"1",     8.5, 12.0, 15.0, "6GHz_20GHz", "3GHz_13GHz",   0, 1, "5p7GHz_13GHz",     3, 7, 0, 0, 0, 7, "mixer", "LSB", "4", "6", "0", 0},
	{"2",    12.0, 16.5, 20.0, "6GHz_20GHz", "3GHz_13GHz",   0, 1, "10p2GHz_19p2GHz",  1, 5, 0, 0, 0, 7, "lo",    "LSB", "4", "6", "0", 1},
	{"3",    16.5, 20.0, 25.0, "6GHz_20GHz", "3GHz_13GHz",   0, 1, "16p5GHz_27p5GHz",  0, 2, 0, 0, 0, 7, "mixer", "USB", "4", "6", "0", 3},
};

// clang-format on

} // namespace

QString scopy::neponset::modeName(NeponsetMode mode)
{
	return mode == NeponsetMode::Bypass ? QStringLiteral("Bypass") : QStringLiteral("LUT");
}

QVector<ModeWrite> scopy::neponset::converterModeWrites(NeponsetMode mode)
{
	// Neponset_Control.txt, ADMV1420 and ADMV1320 blocks.
	if(mode == NeponsetMode::Bypass) {
		return {
			{"bypass_gain_table_en", "1"},
			{"filter_load_en", "0"},
			{"filter_table_en", "0"},
			{"gain_load_en", "0"},
			{"gain_table_en", "0"},
		};
	}

	return {
		{"bypass_gain_table_en", "0"},
		{"filter_load_en", "1"},
		{"filter_table_en", "1"},
		{"gain_load_en", "1"},
		{"gain_table_en", "1"},
	};
}

QVector<ModeWrite> scopy::neponset::tunableFilterModeWrites(NeponsetMode mode)
{
	// Neponset_Control.txt, ADMV8809 and ADMV8827 blocks.
	if(mode == NeponsetMode::Bypass) {
		return {
			{"lut_bypass", "1"},
			{"lut_latch", "0"},
			{"lut_mode", "spi"},
		};
	}

	return {
		{"lut_bypass", "0"},
		{"lut_latch", "0"},
		{"lut_mode", "parallel"},
	};
}

QVector<ModeWrite> scopy::neponset::wideFilterModeWrites(NeponsetMode mode)
{
	// Neponset_Control.txt sets admv8909 ps_en, but that attribute is absent from the
	// pyadi-iio driver. The applier probes for it and records a note when it is missing,
	// rather than failing the whole mode change.
	return {{"ps_en", mode == NeponsetMode::Bypass ? "1" : "0"}};
}

const QVector<RxBandConfig> &scopy::neponset::rxBands() { return RxBandTable; }

const QVector<TxBandConfig> &scopy::neponset::txBands() { return TxBandTable; }

const RxBandConfig *scopy::neponset::rxBandByName(const QString &name)
{
	for(const RxBandConfig &band : RxBandTable) {
		if(name == QLatin1String(band.name)) {
			return &band;
		}
	}
	return nullptr;
}

const TxBandConfig *scopy::neponset::txBandByName(const QString &name)
{
	for(const TxBandConfig &band : TxBandTable) {
		if(name == QLatin1String(band.name)) {
			return &band;
		}
	}
	return nullptr;
}

QString scopy::neponset::bandLabel(const char *name, double startGHz, double endGHz)
{
	return QString("%1  (%2 - %3 GHz)").arg(name).arg(startGHz, 0, 'g', 4).arg(endGHz, 0, 'g', 4);
}

QString scopy::neponset::rxConvLoX3Filter(double loGHz)
{
	// ADMV1420 x3_filter enum: 8GHz_10GHz, 10GHz_12GHz, 12GHz_14GHz, 14GHz_18GHz,
	// 18GHz_24GHz, 24GHz_28GHz.
	if(loGHz < 10.0) {
		return QStringLiteral("8GHz_10GHz");
	}
	if(loGHz < 12.0) {
		return QStringLiteral("10GHz_12GHz");
	}
	if(loGHz < 14.0) {
		return QStringLiteral("12GHz_14GHz");
	}
	if(loGHz < 18.0) {
		return QStringLiteral("14GHz_18GHz");
	}
	if(loGHz < 24.0) {
		return QStringLiteral("18GHz_24GHz");
	}
	return QStringLiteral("24GHz_28GHz");
}

QString scopy::neponset::txConvLoX3Filter(double loGHz)
{
	// ADMV1320 x3_filter enum: 8GHz_9GHz, 9GHz_11GHz, 11GHz_13GHz, 13GHz_17GHz,
	// 17GHz_23GHz, 23GHz_28GHz. Different breakpoints to the ADMV1420, hence a separate
	// mapping for the same physical LO.
	if(loGHz < 9.0) {
		return QStringLiteral("8GHz_9GHz");
	}
	if(loGHz < 11.0) {
		return QStringLiteral("9GHz_11GHz");
	}
	if(loGHz < 13.0) {
		return QStringLiteral("11GHz_13GHz");
	}
	if(loGHz < 17.0) {
		return QStringLiteral("13GHz_17GHz");
	}
	if(loGHz < 23.0) {
		return QStringLiteral("17GHz_23GHz");
	}
	return QStringLiteral("23GHz_28GHz");
}

QStringList scopy::neponset::enumAliases(const QString &value)
{
	if(value == QLatin1String("through")) {
		return {QStringLiteral("bypass")};
	}
	if(value == QLatin1String("bypass")) {
		return {QStringLiteral("through")};
	}
	return {};
}
