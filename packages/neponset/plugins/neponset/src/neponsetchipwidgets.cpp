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

#include "neponsetchipwidgets.h"
#include "luttablewidget.h"
#include "neponsettopology.h"

#include <QLabel>
#include <QLoggingCategory>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

#include <iio-widgets/iiowidgetgroup.h>

Q_LOGGING_CATEGORY(CAT_NEPONSET_WIDGETS, "NeponsetChipWidgets")

using namespace scopy;
using namespace scopy::neponset;

namespace {

constexpr int GridColumns = 4;
constexpr int MonitorIntervalMs = 2000;

} // namespace

// --- GridFiller ---

void GridFiller::add(QWidget *widget)
{
	if(!widget || !m_layout) {
		return;
	}
	m_layout->addWidget(widget, m_index / m_columns, m_index % m_columns);
	m_index++;
}

// --- NeponsetChipWidgets ---

NeponsetChipWidgets::NeponsetChipWidgets(iio_context *ctx, IIOWidgetGroup *group, QObject *parent)
	: QObject(parent)
	, m_ctx(ctx)
	, m_group(group)
{}

NeponsetChipWidgets::~NeponsetChipWidgets() {}

MenuSectionCollapseWidget *NeponsetChipWidgets::makeSection(const QString &title, QWidget *parent, bool collapsed)
{
	MenuSectionCollapseWidget *section = new MenuSectionCollapseWidget(
		title, MenuCollapseSection::MHCW_ARROW, MenuCollapseSection::MHW_BASEWIDGET, parent);
	section->contentLayout()->setSpacing(8);
	section->setCollapsed(collapsed);
	return section;
}

IIOWidget *NeponsetChipWidgets::deviceCombo(iio_device *dev, const char *attribute, const QString &title,
					    const QString &explicitOptions, const QString &info)
{
	if(!dev || !iio_device_find_attr(dev, attribute)) {
		return nullptr;
	}

	const QString availableName = QString(attribute) + "_available";
	const bool hasAvailable = iio_device_find_attr(dev, availableName.toStdString().c_str()) != nullptr;
	if(!hasAvailable && explicitOptions.isEmpty()) {
		qDebug(CAT_NEPONSET_WIDGETS) << deviceName(dev) << attribute
					     << "has no _available attribute and no explicit options, skipping";
		return nullptr;
	}

	IIOWidgetBuilder builder(nullptr);
	builder.device(dev).attribute(attribute).title(title).uiStrategy(IIOWidgetBuilder::ComboUi).group(m_group);
	if(hasAvailable) {
		builder.optionsAttribute(availableName);
	} else {
		builder.optionsValues(explicitOptions);
	}
	if(!info.isEmpty()) {
		builder.infoMessage(info);
	}

	IIOWidget *widget = builder.buildSingle();
	if(widget) {
		connect(this, &NeponsetChipWidgets::readRequested, widget, &IIOWidget::readAsync);
	}
	return widget;
}

IIOWidget *NeponsetChipWidgets::deviceCheck(iio_device *dev, const char *attribute, const QString &title,
					    const QString &info)
{
	if(!dev || !iio_device_find_attr(dev, attribute)) {
		return nullptr;
	}

	IIOWidgetBuilder builder(nullptr);
	builder.device(dev).attribute(attribute).title(title).uiStrategy(IIOWidgetBuilder::CheckBoxUi).group(m_group);
	if(!info.isEmpty()) {
		builder.infoMessage(info);
	}

	IIOWidget *widget = builder.buildSingle();
	if(widget) {
		widget->showProgressBar(false);
		connect(this, &NeponsetChipWidgets::readRequested, widget, &IIOWidget::readAsync);
	}
	return widget;
}

IIOWidget *NeponsetChipWidgets::deviceRange(iio_device *dev, const char *attribute, const QString &title, int min,
					    int max, int step, const QString &info)
{
	if(!dev || !iio_device_find_attr(dev, attribute)) {
		return nullptr;
	}

	IIOWidgetBuilder builder(nullptr);
	builder.device(dev)
		.attribute(attribute)
		.title(title)
		.uiStrategy(IIOWidgetBuilder::RangeUi)
		.optionsValues(QString("%1 %2 %3").arg(min).arg(max).arg(step))
		.group(m_group);
	if(!info.isEmpty()) {
		builder.infoMessage(info);
	}

	IIOWidget *widget = builder.buildSingle();
	if(widget) {
		connect(this, &NeponsetChipWidgets::readRequested, widget, &IIOWidget::readAsync);
	}
	return widget;
}

IIOWidget *NeponsetChipWidgets::channelCombo(iio_channel *chn, const char *attribute, const QString &title,
					     const QString &explicitOptions, const QString &info)
{
	if(!chn || !iio_channel_find_attr(chn, attribute)) {
		return nullptr;
	}

	const QString availableName = QString(attribute) + "_available";
	const bool hasAvailable = iio_channel_find_attr(chn, availableName.toStdString().c_str()) != nullptr;
	if(!hasAvailable && explicitOptions.isEmpty()) {
		return nullptr;
	}

	IIOWidgetBuilder builder(nullptr);
	builder.channel(chn).attribute(attribute).title(title).uiStrategy(IIOWidgetBuilder::ComboUi).group(m_group);
	if(hasAvailable) {
		builder.optionsAttribute(availableName);
	} else {
		builder.optionsValues(explicitOptions);
	}
	if(!info.isEmpty()) {
		builder.infoMessage(info);
	}

	IIOWidget *widget = builder.buildSingle();
	if(widget) {
		connect(this, &NeponsetChipWidgets::readRequested, widget, &IIOWidget::readAsync);
	}
	return widget;
}

IIOWidget *NeponsetChipWidgets::channelCheck(iio_channel *chn, const char *attribute, const QString &title,
					     const QString &info)
{
	if(!chn || !iio_channel_find_attr(chn, attribute)) {
		return nullptr;
	}

	IIOWidgetBuilder builder(nullptr);
	builder.channel(chn).attribute(attribute).title(title).uiStrategy(IIOWidgetBuilder::CheckBoxUi).group(m_group);
	if(!info.isEmpty()) {
		builder.infoMessage(info);
	}

	IIOWidget *widget = builder.buildSingle();
	if(widget) {
		widget->showProgressBar(false);
		connect(this, &NeponsetChipWidgets::readRequested, widget, &IIOWidget::readAsync);
	}
	return widget;
}

IIOWidget *NeponsetChipWidgets::channelRange(iio_channel *chn, const char *attribute, const QString &title, int min,
					     int max, int step, const QString &info)
{
	if(!chn || !iio_channel_find_attr(chn, attribute)) {
		return nullptr;
	}

	IIOWidgetBuilder builder(nullptr);
	builder.channel(chn)
		.attribute(attribute)
		.title(title)
		.uiStrategy(IIOWidgetBuilder::RangeUi)
		.optionsValues(QString("%1 %2 %3").arg(min).arg(max).arg(step))
		.group(m_group);
	if(!info.isEmpty()) {
		builder.infoMessage(info);
	}

	IIOWidget *widget = builder.buildSingle();
	if(widget) {
		connect(this, &NeponsetChipWidgets::readRequested, widget, &IIOWidget::readAsync);
	}
	return widget;
}

IIOWidget *NeponsetChipWidgets::channelMonitor(iio_channel *chn, const char *attribute, const QString &title,
					       int intervalMs, QWidget *parent)
{
	if(!chn || !iio_channel_find_attr(chn, attribute)) {
		return nullptr;
	}

	IIOWidget *widget = IIOWidgetBuilder(parent)
				    .channel(chn)
				    .attribute(attribute)
				    .title(title)
				    .compactMode(true)
				    .group(m_group)
				    .buildSingle();
	if(!widget) {
		return nullptr;
	}

	widget->setEnabled(false);
	widget->showProgressBar(false);
	connect(this, &NeponsetChipWidgets::readRequested, widget, &IIOWidget::readAsync);

	// Stop polling after two consecutive failures so a disconnected board does not get
	// hammered with reads that cannot succeed.
	QTimer *timer = new QTimer(widget);
	auto failures = std::make_shared<int>(0);
	connect(timer, &QTimer::timeout, widget, &IIOWidget::readAsync);
	connect(widget, &IIOWidget::currentStateChanged, timer, [timer, failures](IIOWidget::State state, QString) {
		if(state == IIOWidget::Error) {
			(*failures)++;
			if(*failures >= 2) {
				timer->stop();
			}
		} else if(state == IIOWidget::Correct) {
			*failures = 0;
		}
	});
	timer->start(intervalMs);

	return widget;
}

QWidget *NeponsetChipWidgets::lutEditor(iio_device *dev, const char *attribute, const QString &title,
					QWidget *parent)
{
	if(!dev || !iio_device_find_attr(dev, attribute)) {
		return nullptr;
	}
	return new LutTableWidget(m_ctx, dev, attribute, title, parent);
}

// --- ADMV1420 ---

QWidget *NeponsetChipWidgets::buildAdmv1420(iio_device *dev, QWidget *parent)
{
	QWidget *root = new QWidget(parent);
	QVBoxLayout *rootLayout = new QVBoxLayout(root);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(8);

	if(!dev) {
		QLabel *missing = new QLabel("ADMV1420 not present in this context.", root);
		rootLayout->addWidget(missing);
		return root;
	}

	iio_channel *rf = convRfChannel(dev);
	iio_channel *ifChan = convIfChannel(dev);
	iio_channel *lo = convLoChannel(dev);

	// RF front end
	MenuSectionCollapseWidget *rfSection = makeSection(QString("%1 - RF").arg(deviceName(dev)), root);
	QGridLayout *rfGrid = new QGridLayout();
	GridFiller rfFill(rfGrid, GridColumns);
	rfFill.add(channelCombo(rf, "band", "RF Band"));
	rfFill.add(channelCombo(rf, "bypass_dsa1_gain", "Bypass DSA1 Gain"));
	rfFill.add(channelCombo(rf, "bypass_dsa2_gain", "Bypass DSA2 Gain"));
	rfFill.add(channelCombo(rf, "bypass_dsa3_gain", "Bypass DSA3 Gain"));
	rfFill.add(channelCheck(rf, "bypass_lpf_en", "Bypass LPF En"));
	rfFill.add(channelCheck(rf, "bypass_hpf_en", "Bypass HPF En"));
	rfFill.add(channelRange(rf, "bypass_lpf_val", "Bypass LPF", 0, 15));
	rfFill.add(channelRange(rf, "bypass_hpf_val", "Bypass HPF", 0, 15));
	rfSection->contentLayout()->addLayout(rfGrid);
	rootLayout->addWidget(rfSection);

	// The direct path registers are the LUT shadow values; only interesting in LUT mode or
	// when chasing a specific register, so they start collapsed.
	MenuSectionCollapseWidget *rfDirect =
		makeSection(QString("%1 - RF direct / LUT shadow").arg(deviceName(dev)), root, true);
	QGridLayout *rfDirectGrid = new QGridLayout();
	GridFiller rfDirectFill(rfDirectGrid, GridColumns);
	rfDirectFill.add(channelCombo(rf, "direct_dsa1_gain", "Direct DSA1 Gain"));
	rfDirectFill.add(channelCombo(rf, "direct_dsa2_gain", "Direct DSA2 Gain"));
	rfDirectFill.add(channelCombo(rf, "direct_dsa3_gain", "Direct DSA3 Gain"));
	rfDirectFill.add(channelRange(rf, "direct_lpf_val", "Direct LPF", 0, 15));
	rfDirectFill.add(channelRange(rf, "direct_hpf_val", "Direct HPF", 0, 15));
	rfDirectFill.add(channelRange(rf, "direct_dsa1_offset", "DSA1 Offset", 0, 15));
	rfDirectFill.add(channelRange(rf, "direct_dsa2_offset", "DSA2 Offset", 0, 3));
	rfDirectFill.add(channelRange(rf, "direct_dsa3_offset", "DSA3 Offset", 0, 15));
	rfDirect->contentLayout()->addLayout(rfDirectGrid);
	rootLayout->addWidget(rfDirect);

	// IF output
	MenuSectionCollapseWidget *ifSection = makeSection(QString("%1 - IF").arg(deviceName(dev)), root);
	QGridLayout *ifGrid = new QGridLayout();
	GridFiller ifFill(ifGrid, GridColumns);
	ifFill.add(channelCombo(ifChan, "band", "IF Band"));
	ifFill.add(channelCombo(ifChan, "mode", "IF Mode"));
	ifFill.add(channelCombo(ifChan, "bypass_dsa4_gain", "Bypass DSA4 Gain"));
	ifFill.add(channelCombo(ifChan, "bypass_dsa5_gain", "Bypass DSA5 Gain"));
	ifFill.add(channelCheck(ifChan, "bypass_lpf_en", "Bypass LPF En"));
	ifFill.add(channelRange(ifChan, "bypass_lpf_val", "Bypass LPF", 0, 15));
	ifFill.add(channelCombo(ifChan, "dsai_0p1db", "DSA I 0.1dB",
				QString(), "I channel trim for image rejection calibration"));
	ifFill.add(channelCombo(ifChan, "dsaq_0p1db", "DSA Q 0.1dB",
				QString(), "Q channel trim for image rejection calibration"));
	ifSection->contentLayout()->addLayout(ifGrid);
	rootLayout->addWidget(ifSection);

	MenuSectionCollapseWidget *ifDirect =
		makeSection(QString("%1 - IF direct / LUT shadow").arg(deviceName(dev)), root, true);
	QGridLayout *ifDirectGrid = new QGridLayout();
	GridFiller ifDirectFill(ifDirectGrid, GridColumns);
	ifDirectFill.add(channelCombo(ifChan, "direct_dsa4_gain", "Direct DSA4 Gain"));
	ifDirectFill.add(channelCombo(ifChan, "direct_dsa5_gain", "Direct DSA5 Gain"));
	ifDirectFill.add(channelRange(ifChan, "direct_lpf_val", "Direct LPF", 0, 15));
	ifDirectFill.add(channelRange(ifChan, "direct_dsa4_offset", "DSA4 Offset", 0, 15));
	ifDirectFill.add(channelRange(ifChan, "direct_dsa5_offset", "DSA5 Offset", 0, 15));
	ifDirect->contentLayout()->addLayout(ifDirectGrid);
	rootLayout->addWidget(ifDirect);

	// LO path
	MenuSectionCollapseWidget *loSection = makeSection(QString("%1 - LO").arg(deviceName(dev)), root);
	QGridLayout *loGrid = new QGridLayout();
	GridFiller loFill(loGrid, GridColumns);
	loFill.add(channelCombo(lo, "sideband", "LO Sideband"));
	loFill.add(channelCombo(lo, "x3_filter", "LO x3 Filter",
				QString(), "Not set by the band presets unless you opt in on the Neponset tool"));
	loFill.add(channelRange(lo, "direct_i_phase_val", "I Phase", 0, 31));
	loFill.add(channelRange(lo, "direct_q_phase_val", "Q Phase", 0, 31));
	loSection->contentLayout()->addLayout(loGrid);
	rootLayout->addWidget(loSection);

	// Table enables and GPO
	MenuSectionCollapseWidget *tableSection =
		makeSection(QString("%1 - Tables and GPO").arg(deviceName(dev)), root);
	QGridLayout *tableGrid = new QGridLayout();
	GridFiller tableFill(tableGrid, GridColumns);
	tableFill.add(deviceCheck(dev, "bypass_gain_table_en", "Bypass Gain Table"));
	tableFill.add(deviceCheck(dev, "filter_table_en", "Filter Table En"));
	tableFill.add(deviceCheck(dev, "filter_load_en", "Filter Load En"));
	tableFill.add(deviceCombo(dev, "filter_table_sel", "Filter Table Sel", "A B"));
	tableFill.add(deviceCheck(dev, "gain_table_en", "Gain Table En"));
	tableFill.add(deviceCheck(dev, "gain_load_en", "Gain Load En"));
	tableFill.add(deviceRange(dev, "direct_gpo_f", "Direct GPO_F", 0, 511, 1,
				  "Bits 0-4 address the ADMV8809 LUT, bits 5-8 the ADMV8827 LUT"));
	tableFill.add(deviceRange(dev, "direct_gpo_g", "Direct GPO_G", 0, 127, 1,
				  "Bit 0 is ADMV8827 LUT bit 4, bit 1 SW_CTRL_IN, bit 3 SW_CTRL_OUT. "
				  "2 selects the filter path, 8 the amplifier path."));
	tableFill.add(deviceRange(dev, "bypass_gpo_g", "Bypass GPO_G", 0, 127));
	tableSection->contentLayout()->addLayout(tableGrid);
	rootLayout->addWidget(tableSection);

	// LUT editors
	MenuSectionCollapseWidget *lutSection =
		makeSection(QString("%1 - Lookup tables").arg(deviceName(dev)), root, true);
	QWidget *filterA = lutEditor(dev, "filter_table_config_A", "Filter table A (32 entries)", lutSection);
	QWidget *filterB = lutEditor(dev, "filter_table_config_B", "Filter table B (32 entries)", lutSection);
	QWidget *gainTable = lutEditor(dev, "gain_table_config", "Gain table (67 entries)", lutSection);
	for(QWidget *editor : {filterA, filterB, gainTable}) {
		if(editor) {
			lutSection->contentLayout()->addWidget(editor);
		}
	}
	rootLayout->addWidget(lutSection);

	// Sensors
	QWidget *monitors = buildMonitors(dev, QString("%1 - Monitors").arg(deviceName(dev)), root);
	if(monitors) {
		rootLayout->addWidget(monitors);
	}

	return root;
}

// --- ADMV1320 ---

QWidget *NeponsetChipWidgets::buildAdmv1320(iio_device *dev, QWidget *parent)
{
	QWidget *root = new QWidget(parent);
	QVBoxLayout *rootLayout = new QVBoxLayout(root);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(8);

	if(!dev) {
		rootLayout->addWidget(new QLabel("ADMV1320 not present in this context.", root));
		return root;
	}

	iio_channel *rf = convRfChannel(dev);
	iio_channel *ifChan = convIfChannel(dev);
	iio_channel *lo = convLoChannel(dev);

	MenuSectionCollapseWidget *rfSection = makeSection(QString("%1 - RF").arg(deviceName(dev)), root);
	QGridLayout *rfGrid = new QGridLayout();
	GridFiller rfFill(rfGrid, GridColumns);
	rfFill.add(channelCombo(rf, "band", "RF Band"));
	rfFill.add(channelCombo(rf, "bypass_dsa1_gain", "Bypass DSA1 Gain"));
	rfFill.add(channelCombo(rf, "bypass_dsa2_gain", "Bypass DSA2 Gain"));
	rfFill.add(channelCheck(rf, "bypass_lpf_en", "Bypass LPF En"));
	rfFill.add(channelCheck(rf, "bypass_hpf_en", "Bypass HPF En"));
	rfFill.add(channelRange(rf, "bypass_lpf_val", "Bypass LPF", 0, 15));
	rfFill.add(channelRange(rf, "bypass_hpf_val", "Bypass HPF", 0, 15));
	rfSection->contentLayout()->addLayout(rfGrid);
	rootLayout->addWidget(rfSection);

	MenuSectionCollapseWidget *rfDirect =
		makeSection(QString("%1 - RF direct / LUT shadow").arg(deviceName(dev)), root, true);
	QGridLayout *rfDirectGrid = new QGridLayout();
	GridFiller rfDirectFill(rfDirectGrid, GridColumns);
	rfDirectFill.add(channelCombo(rf, "direct_dsa1_gain", "Direct DSA1 Gain"));
	rfDirectFill.add(channelCombo(rf, "direct_dsa2_gain", "Direct DSA2 Gain"));
	rfDirectFill.add(channelRange(rf, "direct_lpf_val", "Direct LPF", 0, 15));
	rfDirectFill.add(channelRange(rf, "direct_hpf_val", "Direct HPF", 0, 15));
	rfDirectFill.add(channelRange(rf, "direct_dsa1_offset", "DSA1 Offset", 0, 15));
	rfDirectFill.add(channelRange(rf, "direct_dsa2_offset", "DSA2 Offset", 0, 15));
	rfDirect->contentLayout()->addLayout(rfDirectGrid);
	rootLayout->addWidget(rfDirect);

	MenuSectionCollapseWidget *ifSection = makeSection(QString("%1 - IF").arg(deviceName(dev)), root);
	QGridLayout *ifGrid = new QGridLayout();
	GridFiller ifFill(ifGrid, GridColumns);
	ifFill.add(channelCombo(ifChan, "band", "IF Band"));
	ifFill.add(channelCombo(ifChan, "mode", "IF Mode"));
	ifFill.add(channelRange(ifChan, "vcm", "IF VCM", 0, 127, 1, "Common mode voltage, 50 mV per LSB"));
	ifFill.add(channelCombo(ifChan, "dsai_0p1db", "DSA I 0.1dB"));
	ifFill.add(channelCombo(ifChan, "dsaq_0p1db", "DSA Q 0.1dB"));
	ifSection->contentLayout()->addLayout(ifGrid);
	rootLayout->addWidget(ifSection);

	MenuSectionCollapseWidget *loSection = makeSection(QString("%1 - LO").arg(deviceName(dev)), root);
	QGridLayout *loGrid = new QGridLayout();
	GridFiller loFill(loGrid, GridColumns);
	loFill.add(channelCombo(lo, "sideband", "LO Sideband"));
	loFill.add(channelCombo(lo, "x3_filter", "LO x3 Filter",
				QString(), "Not set by the band presets unless you opt in on the Neponset tool"));
	loFill.add(channelRange(lo, "direct_i_phase_val", "I Phase", 0, 31));
	loFill.add(channelRange(lo, "direct_q_phase_val", "Q Phase", 0, 31));
	loFill.add(channelRange(lo, "direct_lon_offset_i", "LON Offset I", 0, 63));
	loFill.add(channelRange(lo, "direct_lon_offset_q", "LON Offset Q", 0, 63));
	loSection->contentLayout()->addLayout(loGrid);
	rootLayout->addWidget(loSection);

	MenuSectionCollapseWidget *tableSection =
		makeSection(QString("%1 - Tables and GPO").arg(deviceName(dev)), root);
	QGridLayout *tableGrid = new QGridLayout();
	GridFiller tableFill(tableGrid, GridColumns);
	tableFill.add(deviceCheck(dev, "bypass_gain_table_en", "Bypass Gain Table"));
	tableFill.add(deviceCheck(dev, "filter_table_en", "Filter Table En"));
	tableFill.add(deviceCheck(dev, "filter_load_en", "Filter Load En"));
	tableFill.add(deviceCombo(dev, "filter_table_sel", "Filter Table Sel", "A B"));
	tableFill.add(deviceCheck(dev, "gain_table_en", "Gain Table En"));
	tableFill.add(deviceCheck(dev, "gain_load_en", "Gain Load En"));
	tableFill.add(deviceCheck(dev, "mixer_bypass_en", "Mixer Bypass En"));
	tableFill.add(deviceRange(dev, "direct_gpo_f", "Direct GPO_F", 0, 511, 1,
				  "Bits 0-2 ADMV8909 LPF, 3-4 HPF, 5-6 SW_IN, 7 SW_OUT, 8 SW_SEL_0"));
	tableFill.add(deviceRange(dev, "direct_gpo_g", "Direct GPO_G", 0, 127, 1,
				  "Bit 0 is SW_SEL_1, bits 1-5 address the ADMV8827 LUT"));
	tableFill.add(deviceRange(dev, "bypass_gpo_g", "Bypass GPO_G", 0, 127));
	// Not in the band spreadsheet, so the presets never touch these - but they gate the GPO
	// pins, so a band that looks like it did nothing on transmit is worth checking here first.
	tableFill.add(deviceRange(dev, "gpo_f_oe", "GPO_F Output En", 0, 511, 1,
				  "Per-bit output enable for GPO_F. The band presets do not set this; if it "
				  "is clear the GPO values have no effect on the pins."));
	tableFill.add(deviceRange(dev, "gpo_g_oe", "GPO_G Output En", 0, 127, 1,
				  "Per-bit output enable for GPO_G. Not set by the band presets."));
	tableSection->contentLayout()->addLayout(tableGrid);
	rootLayout->addWidget(tableSection);

	MenuSectionCollapseWidget *lutSection =
		makeSection(QString("%1 - Lookup tables").arg(deviceName(dev)), root, true);
	QWidget *filterA = lutEditor(dev, "filter_table_config_A", "Filter table A (32 entries)", lutSection);
	QWidget *filterB = lutEditor(dev, "filter_table_config_B", "Filter table B (32 entries)", lutSection);
	QWidget *gainTable = lutEditor(dev, "gain_table_config", "Gain table (67 entries)", lutSection);
	for(QWidget *editor : {filterA, filterB, gainTable}) {
		if(editor) {
			lutSection->contentLayout()->addWidget(editor);
		}
	}
	rootLayout->addWidget(lutSection);

	QWidget *monitors = buildMonitors(dev, QString("%1 - Monitors").arg(deviceName(dev)), root);
	if(monitors) {
		rootLayout->addWidget(monitors);
	}

	return root;
}

// --- ADMV8827 / ADMV8809 ---

QWidget *NeponsetChipWidgets::buildTunableFilter(iio_device *dev, const QString &title, QWidget *parent)
{
	QWidget *root = new QWidget(parent);
	QVBoxLayout *rootLayout = new QVBoxLayout(root);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(8);

	if(!dev) {
		rootLayout->addWidget(new QLabel(QString("%1 not present in this context.").arg(title), root));
		return root;
	}

	iio_channel *rf = filterRfChannel(dev);

	MenuSectionCollapseWidget *section = makeSection(QString("%1 (%2)").arg(title, deviceName(dev)), root);
	QGridLayout *grid = new QGridLayout();
	GridFiller fill(grid, GridColumns);
	fill.add(channelCombo(rf, "band", "Band"));
	fill.add(channelRange(rf, "direct_hpf_val", "Direct HPF", 0, 7));
	fill.add(channelRange(rf, "direct_lpf_val", "Direct LPF", 0, 7));
	fill.add(deviceCheck(dev, "lut_bypass", "LUT Bypass",
			     "Bypass mode sets this so band, HPF and LPF come straight from SPI"));
	fill.add(deviceCheck(dev, "lut_latch", "LUT Latch"));
	fill.add(deviceCombo(dev, "lut_mode", "LUT Mode"));
	fill.add(deviceCombo(dev, "lut_ab_sel", "LUT A/B Sel", "A B"));
	fill.add(deviceRange(dev, "lut_entry", "LUT Entry", 0, 31));
	section->contentLayout()->addLayout(grid);
	rootLayout->addWidget(section);

	MenuSectionCollapseWidget *lutSection = makeSection(QString("%1 - Lookup tables").arg(title), root, true);
	QWidget *lutA = lutEditor(dev, "lut_config_A", "LUT A (32 entries)", lutSection);
	QWidget *lutB = lutEditor(dev, "lut_config_B", "LUT B (32 entries)", lutSection);
	for(QWidget *editor : {lutA, lutB}) {
		if(editor) {
			lutSection->contentLayout()->addWidget(editor);
		}
	}
	rootLayout->addWidget(lutSection);

	return root;
}

// --- ADMV8909 ---

QWidget *NeponsetChipWidgets::buildAdmv8909(iio_device *dev, QWidget *parent)
{
	QWidget *root = new QWidget(parent);
	QVBoxLayout *rootLayout = new QVBoxLayout(root);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(8);

	if(!dev) {
		rootLayout->addWidget(new QLabel("ADMV8909 not present in this context.", root));
		return root;
	}

	iio_channel *rf = filterRfChannel(dev);

	MenuSectionCollapseWidget *section =
		makeSection(QString("ADMV8909 wideband filter (%1)").arg(deviceName(dev)), root);
	QGridLayout *grid = new QGridLayout();
	GridFiller fill(grid, GridColumns);
	fill.add(channelRange(rf, "direct_sw_in_val", "Switch In", 0, 3,
			      1, "On the current ADMV8909 rev SW_IN and SW_OUT are swapped relative to "
				 "the datasheet; a future rev will match it"));
	fill.add(channelRange(rf, "direct_sw_out_val", "Switch Out", 0, 1));
	fill.add(channelRange(rf, "direct_hpf_val", "Direct HPF", 0, 3));
	fill.add(channelRange(rf, "direct_lpf_val", "Direct LPF", 0, 7));
	// Present on the board but absent from the pyadi-iio driver, so it only appears when the
	// running driver actually exposes it.
	fill.add(deviceCheck(dev, "ps_en", "Parallel Select En",
			     "Set in bypass mode, cleared in LUT mode (Neponset_Control.txt)"));
	section->contentLayout()->addLayout(grid);
	rootLayout->addWidget(section);

	MenuSectionCollapseWidget *latchSection =
		makeSection(QString("ADMV8909 fast latch (%1)").arg(deviceName(dev)), root, true);
	QGridLayout *latchGrid = new QGridLayout();
	GridFiller latchFill(latchGrid, GridColumns);
	latchFill.add(deviceRange(dev, "fast_latch_pointer", "Latch Pointer", 0, 63));
	latchFill.add(deviceRange(dev, "fast_latch_start", "Latch Start", 0, 63));
	latchFill.add(deviceRange(dev, "fast_latch_stop", "Latch Stop", 0, 63));
	latchFill.add(deviceCheck(dev, "fast_latch_direction", "Reverse Sweep"));
	latchFill.add(deviceCheck(dev, "fast_latch_load", "Latch Load",
				  "Write-only trigger: setting this loads the LUT entry at the pointer"));
	latchSection->contentLayout()->addLayout(latchGrid);
	rootLayout->addWidget(latchSection);

	MenuSectionCollapseWidget *lutSection = makeSection("ADMV8909 - Lookup table", root, true);
	QWidget *lut = lutEditor(dev, "lut_config", "LUT (64 entries)", lutSection);
	if(lut) {
		lutSection->contentLayout()->addWidget(lut);
		rootLayout->addWidget(lutSection);
	} else {
		lutSection->deleteLater();
	}

	return root;
}

// --- HMC7003 ---

QWidget *NeponsetChipWidgets::buildHmc7003(iio_device *dev, QWidget *parent)
{
	QWidget *root = new QWidget(parent);
	QVBoxLayout *rootLayout = new QVBoxLayout(root);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(8);

	if(!dev) {
		rootLayout->addWidget(new QLabel("HMC7003 not present in this context.", root));
		return root;
	}

	iio_channel *out = mixerChannel(dev);

	MenuSectionCollapseWidget *section = makeSection(QString("HMC7003 LO mixer (%1)").arg(deviceName(dev)), root);
	QGridLayout *grid = new QGridLayout();
	GridFiller fill(grid, GridColumns);
	fill.add(channelCombo(out, "feedthru", "Feedthrough",
			      QString(), "\"lo\" passes the LO straight through, bypassing the mixer. The band "
					 "spreadsheet calls this \"Through\" and uses it for the 12-16.5 GHz "
					 "band."));
	fill.add(channelCombo(out, "sideband", "Sideband"));
	fill.add(channelCombo(out, "divider", "Divider"));
	fill.add(channelCombo(out, "filter", "LO Filter"));
	fill.add(channelCombo(out, "hardwaregain", "Gain (dB)"));
	fill.add(channelCombo(out, "phase", "Phase (deg)"));
	section->contentLayout()->addLayout(grid);
	rootLayout->addWidget(section);

	return root;
}

// --- generic and monitors ---

QWidget *NeponsetChipWidgets::buildGenericDevice(iio_device *dev, const QString &title, QWidget *parent)
{
	QWidget *root = new QWidget(parent);
	QVBoxLayout *rootLayout = new QVBoxLayout(root);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(8);

	if(!dev) {
		rootLayout->addWidget(new QLabel(QString("%1 not present in this context.").arg(title), root));
		return root;
	}

	MenuSectionCollapseWidget *section = makeSection(QString("%1 (%2)").arg(title, deviceName(dev)), root);

	QList<IIOWidget *> widgets = IIOWidgetBuilder(section).device(dev).group(m_group).buildAll();

	QGridLayout *grid = new QGridLayout();
	GridFiller fill(grid, GridColumns);
	for(IIOWidget *widget : widgets) {
		connect(this, &NeponsetChipWidgets::readRequested, widget, &IIOWidget::readAsync);
		fill.add(widget);
	}

	if(fill.isEmpty()) {
		section->contentLayout()->addWidget(
			new QLabel(QString("%1 exposes no attributes.").arg(deviceName(dev)), section));
		delete grid;
	} else {
		section->contentLayout()->addLayout(grid);
	}

	rootLayout->addWidget(section);
	return root;
}

QWidget *NeponsetChipWidgets::buildMonitors(iio_device *dev, const QString &title, QWidget *parent)
{
	if(!dev) {
		return nullptr;
	}

	iio_channel *temp = findChannel(dev, "temp0", false);
	iio_channel *power = findChannel(dev, "power0", false);
	if(!temp && !power) {
		return nullptr;
	}

	MenuSectionCollapseWidget *section = makeSection(title, parent);
	QGridLayout *grid = new QGridLayout();
	GridFiller fill(grid, GridColumns);

	// Raw counts only: the driver exposes no scale or offset and there is no documented
	// conversion, so these are deliberately not labelled as degrees or dBm.
	fill.add(channelMonitor(temp, "raw", "Temp (raw)", MonitorIntervalMs, section));
	fill.add(channelMonitor(power, "raw", "Power det (raw)", MonitorIntervalMs, section));

	if(fill.isEmpty()) {
		section->deleteLater();
		delete grid;
		return nullptr;
	}

	section->contentLayout()->addLayout(grid);
	return section;
}

#include "moc_neponsetchipwidgets.cpp"
