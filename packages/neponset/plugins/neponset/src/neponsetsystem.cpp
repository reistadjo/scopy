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

#include "neponsetsystem.h"

#include <QFontDatabase>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLoggingCategory>
#include <QScrollArea>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <pluginbase/statusbarmanager.h>
#include <style.h>
#include <menucollapsesection.h>
#include <menusectionwidget.h>

Q_LOGGING_CATEGORY(CAT_NEPONSET_SYSTEM, "NeponsetSystem")

using namespace scopy;
using namespace scopy::neponset;

namespace {

constexpr int StatusDisplayMs = 6000;

} // namespace

NeponsetSystem::NeponsetSystem(NeponsetTopology *topology, NeponsetApplier *applier, IIOWidgetGroup *group,
			       QWidget *parent)
	: QWidget(parent)
	, m_topology(topology)
	, m_applier(applier)
	, m_group(group)
{
	m_chips = new NeponsetChipWidgets(topology ? topology->context() : nullptr, group, this);
	connect(this, &NeponsetSystem::readRequested, m_chips, &NeponsetChipWidgets::readRequested);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	m_tool = new ToolTemplate(this);
	m_tool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_tool->topContainer()->setVisible(true);
	m_tool->topContainerMenuControl()->setVisible(false);
	mainLayout->addWidget(m_tool);

	m_refreshButton = new AnimatedRefreshBtn(false, this);
	m_tool->addWidgetToTopContainerHelper(m_refreshButton, TTA_RIGHT);
	connect(m_refreshButton, &QPushButton::clicked, this, &NeponsetSystem::refresh);

	QWidget *content = new QWidget(this);
	QVBoxLayout *contentLayout = new QVBoxLayout(content);
	contentLayout->setContentsMargins(6, 6, 6, 6);
	contentLayout->setSpacing(8);

	contentLayout->addWidget(buildModeSection(content));
	contentLayout->addWidget(buildBandSection(content));
	contentLayout->addWidget(buildLogSection(content));
	contentLayout->addWidget(buildBoardSection(content));
	contentLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Preferred, QSizePolicy::Expanding));

	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setWidget(content);
	m_tool->addWidgetToCentralContainerHelper(scroll);

	if(m_topology && !m_topology->missingDevices().isEmpty()) {
		m_log->appendPlainText(QString("Devices not found in this context: %1")
					       .arg(m_topology->missingDevices().join(", ")));
	}

	for(int i = 0; i < PathCount; ++i) {
		updateLoSummary(i);
	}
	updateModeSummary();
}

NeponsetSystem::~NeponsetSystem() {}

// --- mode ---

QWidget *NeponsetSystem::buildModeSection(QWidget *parent)
{
	MenuSectionCollapseWidget *section = new MenuSectionCollapseWidget(
		"Operating mode", MenuCollapseSection::MHCW_ARROW, MenuCollapseSection::MHW_BASEWIDGET, parent);

	QLabel *blurb = new QLabel(
		"Bypass mode drives every converter and filter over SPI, which is what the band presets "
		"below use and what you want for bench work. LUT mode hands filter and gain selection to "
		"the on-chip lookup tables, addressed in parallel from the mezzanine connector for fast "
		"hopping.",
		section);
	blurb->setWordWrap(true);
	section->contentLayout()->addWidget(blurb);

	QHBoxLayout *row = new QHBoxLayout();
	row->setSpacing(8);

	QLabel *scopeLabel = new QLabel("Apply to", section);
	m_modePathSelect = new QComboBox(section);
	m_modePathSelect->addItem("All paths", -1);
	for(int i = 0; i < PathCount; ++i) {
		m_modePathSelect->addItem(QString("Path %1").arg(i), i);
	}

	QPushButton *bypassBtn = new QPushButton("Set bypass mode", section);
	QPushButton *lutBtn = new QPushButton("Set LUT mode", section);
	Style::setStyle(bypassBtn, style::properties::button::basicButton);
	Style::setStyle(lutBtn, style::properties::button::basicButton);

	connect(bypassBtn, &QPushButton::clicked, this, [this]() {
		applyMode(NeponsetMode::Bypass, m_modePathSelect->currentData().toInt());
	});
	connect(lutBtn, &QPushButton::clicked, this,
		[this]() { applyMode(NeponsetMode::Lut, m_modePathSelect->currentData().toInt()); });

	row->addWidget(scopeLabel);
	row->addWidget(m_modePathSelect);
	row->addWidget(bypassBtn);
	row->addWidget(lutBtn);
	row->addStretch();
	section->contentLayout()->addLayout(row);

	m_modeSummary = new QLabel(section);
	m_modeSummary->setWordWrap(true);
	section->contentLayout()->addWidget(m_modeSummary);

	return section;
}

QString NeponsetSystem::detectedMode(int pathIndex) const
{
	if(!m_applier) {
		return QStringLiteral("unknown");
	}
	return m_applier->detectMode(pathIndex);
}

void NeponsetSystem::updateModeSummary()
{
	if(!m_modeSummary) {
		return;
	}

	QStringList parts;
	bool anyMixed = false;
	for(int i = 0; i < PathCount; ++i) {
		const QString mode = detectedMode(i);
		if(mode == "mixed") {
			anyMixed = true;
		}
		parts << QString("path %1: %2").arg(i).arg(mode);
	}

	m_modeSummary->setText("Read back - " + parts.join(", "));
	// "mixed" means the table enables and the filter LUT bypass bits disagree, which is what a
	// mode change that only partly landed looks like. Worth flagging rather than averaging away.
	m_modeSummary->setStyleSheet(
		anyMixed ? QString("color: %1;").arg(Style::getAttribute(json::theme::content_error)) : QString());
}

void NeponsetSystem::applyMode(NeponsetMode mode, int pathIndex)
{
	if(!m_applier) {
		return;
	}

	const ApplyResult result = m_applier->applyMode(mode, pathIndex);
	const QString scope = pathIndex < 0 ? QString("all paths") : QString("path %1").arg(pathIndex);
	showResult(result, QString("%1 mode on %2").arg(modeName(mode), scope));
}

// --- bands ---

QWidget *NeponsetSystem::buildBandSection(QWidget *parent)
{
	MenuSectionCollapseWidget *section = new MenuSectionCollapseWidget(
		"Band presets", MenuCollapseSection::MHCW_ARROW, MenuCollapseSection::MHW_BASEWIDGET, parent);

	QLabel *blurb = new QLabel(
		"Each selection writes one row of the Neponset band mapping spreadsheet: converter RF and "
		"IF bands, GPO switch controls, both tunable filters and the LO mixer. Receive and transmit "
		"on a channel share an LO, so their bands have to imply the same LO frequency - the LO "
		"column flags it when they do not.",
		section);
	blurb->setWordWrap(true);
	section->contentLayout()->addWidget(blurb);

	QGridLayout *grid = new QGridLayout();
	grid->setSpacing(6);

	int col = 0;
	for(const QString &heading : {QString("Channel"), QString("RX band"), QString("TX band"), QString("LO"),
				      QString()}) {
		QLabel *label = new QLabel(heading, section);
		Style::setStyle(label, style::properties::label::menuMedium);
		grid->addWidget(label, 0, col++);
	}

	for(int i = 0; i < PathCount; ++i) {
		ChannelRow &row = m_rows[i];

		QLabel *name = new QLabel(QString("Channel %1").arg(i), section);

		row.rxBand = new QComboBox(section);
		row.rxBand->addItem("(leave unchanged)", QString());
		for(const RxBandConfig &band : rxBands()) {
			row.rxBand->addItem(bandLabel(band.name, band.startGHz, band.endGHz), QString(band.name));
		}

		row.txBand = new QComboBox(section);
		row.txBand->addItem("(leave unchanged)", QString());
		for(const TxBandConfig &band : txBands()) {
			row.txBand->addItem(bandLabel(band.name, band.startGHz, band.endGHz), QString(band.name));
		}

		row.loSummary = new QLabel(section);
		row.loSummary->setWordWrap(true);
		row.loSummary->setMinimumWidth(220);

		row.applyBtn = new QPushButton("Apply", section);
		Style::setStyle(row.applyBtn, style::properties::button::basicButton);

		connect(row.rxBand, &QComboBox::currentTextChanged, this, [this, i]() { updateLoSummary(i); });
		connect(row.txBand, &QComboBox::currentTextChanged, this, [this, i]() { updateLoSummary(i); });
		connect(row.applyBtn, &QPushButton::clicked, this, [this, i]() { applyChannel(i); });

		grid->addWidget(name, i + 1, 0);
		grid->addWidget(row.rxBand, i + 1, 1);
		grid->addWidget(row.txBand, i + 1, 2);
		grid->addWidget(row.loSummary, i + 1, 3);
		grid->addWidget(row.applyBtn, i + 1, 4);
	}

	section->contentLayout()->addLayout(grid);

	QHBoxLayout *footer = new QHBoxLayout();
	footer->setSpacing(8);

	m_deriveLoX3 = new QCheckBox("Also set the converter LO x3 filter", section);
	m_deriveLoX3->setChecked(false);
	m_deriveLoX3->setToolTip("The x3 filter is not in the spreadsheet. With this ticked it is derived from "
				 "the band's LO frequency and flagged as derived in the log.");

	QPushButton *applyAll = new QPushButton("Apply all channels", section);
	Style::setStyle(applyAll, style::properties::button::basicButton);
	connect(applyAll, &QPushButton::clicked, this, &NeponsetSystem::applyAllChannels);

	footer->addWidget(m_deriveLoX3);
	footer->addStretch();
	footer->addWidget(applyAll);
	section->contentLayout()->addLayout(footer);

	return section;
}

void NeponsetSystem::updateLoSummary(int pathIndex)
{
	if(pathIndex < 0 || pathIndex >= PathCount) {
		return;
	}

	ChannelRow &row = m_rows[pathIndex];
	if(!row.loSummary) {
		return;
	}

	const RxBandConfig *rx = rxBandByName(row.rxBand->currentData().toString());
	const TxBandConfig *tx = txBandByName(row.txBand->currentData().toString());

	QString text;
	bool conflict = false;

	if(rx && tx) {
		if(qFuzzyCompare(rx->loGHz, tx->loGHz)) {
			text = QString("%1 GHz").arg(rx->loGHz);
		} else {
			conflict = true;
			text = QString("conflict: RX needs %1 GHz, TX needs %2 GHz").arg(rx->loGHz).arg(tx->loGHz);
		}
	} else if(rx) {
		text = QString("%1 GHz (RX only)").arg(rx->loGHz);
	} else if(tx) {
		text = QString("%1 GHz (TX only)").arg(tx->loGHz);
	} else {
		text = "-";
	}

	row.loSummary->setText(text);
	row.loSummary->setStyleSheet(conflict ? QString("color: %1;").arg(Style::getAttribute(json::theme::content_error))
					      : QString());
}

bool NeponsetSystem::selectRxBand(int pathIndex, const QString &bandName)
{
	if(pathIndex < 0 || pathIndex >= PathCount || !m_rows[pathIndex].rxBand) {
		return false;
	}
	const int index = m_rows[pathIndex].rxBand->findData(bandName);
	if(index < 0) {
		return false;
	}
	m_rows[pathIndex].rxBand->setCurrentIndex(index);
	return true;
}

bool NeponsetSystem::selectTxBand(int pathIndex, const QString &bandName)
{
	if(pathIndex < 0 || pathIndex >= PathCount || !m_rows[pathIndex].txBand) {
		return false;
	}
	const int index = m_rows[pathIndex].txBand->findData(bandName);
	if(index < 0) {
		return false;
	}
	m_rows[pathIndex].txBand->setCurrentIndex(index);
	return true;
}

QString NeponsetSystem::selectedRxBand(int pathIndex) const
{
	if(pathIndex < 0 || pathIndex >= PathCount || !m_rows[pathIndex].rxBand) {
		return QString();
	}
	return m_rows[pathIndex].rxBand->currentData().toString();
}

QString NeponsetSystem::selectedTxBand(int pathIndex) const
{
	if(pathIndex < 0 || pathIndex >= PathCount || !m_rows[pathIndex].txBand) {
		return QString();
	}
	return m_rows[pathIndex].txBand->currentData().toString();
}

void NeponsetSystem::applyChannel(int pathIndex)
{
	if(!m_applier || pathIndex < 0 || pathIndex >= PathCount) {
		return;
	}

	const RxBandConfig *rx = rxBandByName(selectedRxBand(pathIndex));
	const TxBandConfig *tx = txBandByName(selectedTxBand(pathIndex));

	if(!rx && !tx) {
		StatusBarManager::pushMessage(QString("Channel %1: pick a band first.").arg(pathIndex),
					      StatusDisplayMs);
		return;
	}

	const bool derive = m_deriveLoX3 && m_deriveLoX3->isChecked();
	const ApplyResult result = m_applier->applyChannel(pathIndex, rx, tx, derive);

	QStringList what;
	if(rx) {
		what << QString("RX %1").arg(rx->name);
	}
	if(tx) {
		what << QString("TX %1").arg(tx->name);
	}
	showResult(result, QString("channel %1 %2").arg(pathIndex).arg(what.join(" / ")));
}

void NeponsetSystem::applyAllChannels()
{
	if(!m_applier) {
		return;
	}

	ApplyResult combined;
	int applied = 0;

	for(int i = 0; i < PathCount; ++i) {
		const RxBandConfig *rx = rxBandByName(selectedRxBand(i));
		const TxBandConfig *tx = txBandByName(selectedTxBand(i));
		if(!rx && !tx) {
			continue;
		}

		const bool derive = m_deriveLoX3 && m_deriveLoX3->isChecked();
		combined.merge(m_applier->applyChannel(i, rx, tx, derive));
		applied++;
	}

	if(applied == 0) {
		StatusBarManager::pushMessage("Pick a band on at least one channel first.", StatusDisplayMs);
		return;
	}

	showResult(combined, QString("%1 channel%2").arg(applied).arg(applied == 1 ? "" : "s"));
}

// --- log and status ---

QWidget *NeponsetSystem::buildLogSection(QWidget *parent)
{
	MenuSectionCollapseWidget *section = new MenuSectionCollapseWidget(
		"Apply log", MenuCollapseSection::MHCW_ARROW, MenuCollapseSection::MHW_BASEWIDGET, parent);

	QLabel *blurb = new QLabel(
		"Every write is checked against what the device offers and read back afterwards. Anything "
		"the spreadsheet asked for that the driver would not take shows up here rather than being "
		"quietly dropped.",
		section);
	blurb->setWordWrap(true);
	section->contentLayout()->addWidget(blurb);

	m_status = new SmallProgressBar(section);
	section->contentLayout()->addWidget(m_status);

	m_log = new QPlainTextEdit(section);
	m_log->setReadOnly(true);
	m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
	m_log->setMinimumHeight(200);
	section->contentLayout()->addWidget(m_log);

	QHBoxLayout *footer = new QHBoxLayout();
	QPushButton *clearBtn = new QPushButton("Clear", section);
	Style::setStyle(clearBtn, style::properties::button::basicButton);
	connect(clearBtn, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
	footer->addStretch();
	footer->addWidget(clearBtn);
	section->contentLayout()->addLayout(footer);

	return section;
}

void NeponsetSystem::showResult(const ApplyResult &result, const QString &what)
{
	const QString headline = QString("%1: %2").arg(what, result.summary());

	if(m_log) {
		m_log->appendPlainText(QString("=== %1 ===").arg(headline));
		const QString detail = result.detail();
		if(!detail.isEmpty()) {
			m_log->appendPlainText(detail);
		}
		m_log->appendPlainText(QString());
	}

	if(m_status) {
		// The bar sits full by default and is used purely as a colour cue, the way the
		// ADRV9002 profile manager uses it.
		m_status->setBarColor(Style::getAttribute(result.hasProblems() ? json::theme::content_error
									      : json::theme::content_success));
	}

	StatusBarManager::pushMessage(headline, StatusDisplayMs);
	qInfo(CAT_NEPONSET_SYSTEM) << headline;

	if(result.hasProblems()) {
		qWarning(CAT_NEPONSET_SYSTEM) << "Apply had problems:\n" << result.detail();
	}

	// Pull the rest of the UI back in sync with what the hardware now reports.
	updateModeSummary();
	Q_EMIT readRequested();
}

QString NeponsetSystem::applyLog() const { return m_log ? m_log->toPlainText() : QString(); }

// --- board devices ---

QWidget *NeponsetSystem::buildBoardSection(QWidget *parent)
{
	MenuSectionCollapseWidget *section = new MenuSectionCollapseWidget(
		"Board status and discrete controls", MenuCollapseSection::MHCW_ARROW,
		MenuCollapseSection::MHW_BASEWIDGET, parent);

	QLabel *blurb = new QLabel(
		"neponset_gpio carries the mezzanine discretes - the filter and gain addresses, the load "
		"strobes and the LO filter selects. No pyadi-iio driver describes them, so these controls "
		"are whatever the running driver publishes rather than a fixed list.",
		section);
	blurb->setWordWrap(true);
	section->contentLayout()->addWidget(blurb);

	if(m_topology) {
		section->contentLayout()->addWidget(
			m_chips->buildGenericDevice(m_topology->status(), "Status", section));
		section->contentLayout()->addWidget(
			m_chips->buildGenericDevice(m_topology->gpio(), "Discrete controls", section));

		for(int i = 0; i < PathCount; ++i) {
			QWidget *monitors = m_chips->buildMonitors(
				m_topology->rx(i).conv, QString("Receive path %1 sensors").arg(i), section);
			if(monitors) {
				section->contentLayout()->addWidget(monitors);
			}
		}
	}

	return section;
}

// --- refresh ---

void NeponsetSystem::refresh()
{
	updateModeSummary();
	m_refreshButton->startAnimation();

	QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
	connect(
		watcher, &QFutureWatcher<void>::finished, this,
		[this, watcher]() {
			m_refreshButton->stopAnimation();
			watcher->deleteLater();
		},
		Qt::QueuedConnection);

	QFuture<void> future = QtConcurrent::run([this]() { Q_EMIT readRequested(); });
	watcher->setFuture(future);
}

#include "moc_neponsetsystem.cpp"
