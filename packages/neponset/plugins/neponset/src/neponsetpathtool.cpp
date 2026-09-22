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

#include "neponsetpathtool.h"

#include <QFutureWatcher>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <style.h>

using namespace scopy;
using namespace scopy::neponset;

// --- NeponsetPathTool ---

NeponsetPathTool::NeponsetPathTool(const QString &buttonPrefix, int pathCount, QWidget *parent)
	: QWidget(parent)
	, m_buttonPrefix(buttonPrefix)
	, m_pathCount(pathCount)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	m_tool = new ToolTemplate(this);
	m_tool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_tool->topContainer()->setVisible(true);
	m_tool->topContainerMenuControl()->setVisible(false);
	mainLayout->addWidget(m_tool);

	m_refreshButton = new AnimatedRefreshBtn(false, this);
	m_tool->addWidgetToTopContainerHelper(m_refreshButton, TTA_RIGHT);
	connect(m_refreshButton, &QPushButton::clicked, this, &NeponsetPathTool::refresh);

	m_stack = new QStackedWidget(this);
	m_tool->addWidgetToCentralContainerHelper(m_stack);

	m_buttons = new QButtonGroup(this);
	m_buttons->setExclusive(true);
}

NeponsetPathTool::~NeponsetPathTool() {}

void NeponsetPathTool::buildPages()
{
	for(int i = 0; i < m_pathCount; ++i) {
		QWidget *page = createPathPage(i, m_stack);

		QScrollArea *scroll = new QScrollArea(m_stack);
		scroll->setWidgetResizable(true);
		scroll->setWidget(page);
		m_stack->addWidget(scroll);

		QPushButton *btn = new QPushButton(QString("%1%2").arg(m_buttonPrefix).arg(i), this);
		btn->setCheckable(true);
		btn->setChecked(i == 0);
		btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
		Style::setStyle(btn, style::properties::button::blueGrayButton);
		connect(btn, &QPushButton::clicked, this, [this, scroll]() { m_stack->setCurrentWidget(scroll); });

		m_buttons->addButton(btn);
		m_tool->addWidgetToTopContainerHelper(btn, TTA_LEFT);
	}
}

void NeponsetPathTool::refresh()
{
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

// --- NeponsetRx ---

NeponsetRx::NeponsetRx(NeponsetTopology *topology, IIOWidgetGroup *group, QWidget *parent)
	: NeponsetPathTool("RX", PathCount, parent)
	, m_topology(topology)
{
	m_chips = new NeponsetChipWidgets(topology ? topology->context() : nullptr, group, this);
	connect(this, &NeponsetPathTool::readRequested, m_chips, &NeponsetChipWidgets::readRequested);

	buildPages();
}

QWidget *NeponsetRx::createPathPage(int index, QWidget *parent)
{
	QWidget *page = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(page);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(8);

	const RxPath path = m_topology ? m_topology->rx(index) : RxPath{};

	QLabel *title = new QLabel(QString("Receive path %1").arg(index), page);
	Style::setStyle(title, style::properties::label::menuBig);
	layout->addWidget(title);

	layout->addWidget(m_chips->buildAdmv1420(path.conv, page));
	layout->addWidget(m_chips->buildTunableFilter(path.filtLow, "ADMV8809 low band filter", page));
	layout->addWidget(m_chips->buildTunableFilter(path.filtHigh, "ADMV8827 high band filter", page));
	layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Preferred, QSizePolicy::Expanding));

	return page;
}

// --- NeponsetTx ---

NeponsetTx::NeponsetTx(NeponsetTopology *topology, IIOWidgetGroup *group, QWidget *parent)
	: NeponsetPathTool("TX", PathCount, parent)
	, m_topology(topology)
{
	m_chips = new NeponsetChipWidgets(topology ? topology->context() : nullptr, group, this);
	connect(this, &NeponsetPathTool::readRequested, m_chips, &NeponsetChipWidgets::readRequested);

	buildPages();
}

QWidget *NeponsetTx::createPathPage(int index, QWidget *parent)
{
	QWidget *page = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(page);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(8);

	const TxPath path = m_topology ? m_topology->tx(index) : TxPath{};

	QLabel *title = new QLabel(QString("Transmit path %1").arg(index), page);
	Style::setStyle(title, style::properties::label::menuBig);
	layout->addWidget(title);

	layout->addWidget(m_chips->buildAdmv1320(path.conv, page));
	layout->addWidget(m_chips->buildTunableFilter(path.filtHigh, "ADMV8827 high band filter", page));
	layout->addWidget(m_chips->buildAdmv8909(path.filtWide, page));
	layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Preferred, QSizePolicy::Expanding));

	return page;
}

// --- NeponsetLo ---

NeponsetLo::NeponsetLo(NeponsetTopology *topology, IIOWidgetGroup *group, QWidget *parent)
	: NeponsetPathTool("LO", PathCount, parent)
	, m_topology(topology)
{
	m_chips = new NeponsetChipWidgets(topology ? topology->context() : nullptr, group, this);
	connect(this, &NeponsetPathTool::readRequested, m_chips, &NeponsetChipWidgets::readRequested);

	buildPages();
}

QWidget *NeponsetLo::createPathPage(int index, QWidget *parent)
{
	QWidget *page = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(page);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(8);

	const LoPath path = m_topology ? m_topology->lo(index) : LoPath{};

	QLabel *title = new QLabel(QString("LO %1").arg(index), page);
	Style::setStyle(title, style::properties::label::menuBig);
	layout->addWidget(title);

	QLabel *note = new QLabel(
		QString("Shared by receive path %1 and transmit path %1, so their bands have to move "
			"together. The LO frequency itself is set upstream of this module - the HMC7003 only "
			"divides, mixes and filters it.")
			.arg(index),
		page);
	note->setWordWrap(true);
	layout->addWidget(note);

	layout->addWidget(m_chips->buildHmc7003(path.mixer, page));
	layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Preferred, QSizePolicy::Expanding));

	return page;
}

#include "moc_neponsetpathtool.cpp"
