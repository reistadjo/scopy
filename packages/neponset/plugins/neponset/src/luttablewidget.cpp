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

#include "luttablewidget.h"
#include "neponsettopology.h"

#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLoggingCategory>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVector>

#include <style.h>

Q_LOGGING_CATEGORY(CAT_NEPONSET_LUT, "NeponsetLutTable")

using namespace scopy;
using namespace scopy::neponset;

namespace {

// Generous enough for the largest table on the module (the 67 entry ADMV1420 gain table) with
// room to spare. libiio's own convenience path uses 1 KiB, which truncates every one of these.
constexpr size_t LutBufferSize = 16384;

// Programming a full table takes noticeably longer than an ordinary attribute write.
constexpr unsigned int LutWriteTimeoutMs = 30000;
constexpr unsigned int DefaultTimeoutMs = 3000;

} // namespace

LutTableWidget::LutTableWidget(iio_context *ctx, iio_device *dev, const QString &attribute, const QString &title,
			       QWidget *parent)
	: QWidget(parent)
	, m_ctx(ctx)
	, m_dev(dev)
	, m_attribute(attribute)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	QLabel *titleLabel = new QLabel(title, this);
	Style::setStyle(titleLabel, style::properties::label::menuMedium);
	layout->addWidget(titleLabel);

	m_editor = new QPlainTextEdit(this);
	m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
	m_editor->setMinimumHeight(180);
	m_editor->setPlaceholderText("Press Read to load the table from the device, or paste one here.");
	layout->addWidget(m_editor);

	QHBoxLayout *buttons = new QHBoxLayout();
	buttons->setContentsMargins(0, 0, 0, 0);
	buttons->setSpacing(6);

	m_readBtn = new QPushButton("Read", this);
	m_writeBtn = new QPushButton("Write", this);
	m_loadBtn = new QPushButton("Load file...", this);
	m_saveBtn = new QPushButton("Save file...", this);

	for(QPushButton *btn : QVector<QPushButton *>{m_readBtn, m_writeBtn, m_loadBtn, m_saveBtn}) {
		Style::setStyle(btn, style::properties::button::basicButton);
		buttons->addWidget(btn);
	}
	buttons->addStretch();
	layout->addLayout(buttons);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	layout->addWidget(m_status);

	connect(m_readBtn, &QPushButton::clicked, this, &LutTableWidget::readTable);
	connect(m_writeBtn, &QPushButton::clicked, this, &LutTableWidget::writeTable);
	connect(m_loadBtn, &QPushButton::clicked, this, &LutTableWidget::loadFromFile);
	connect(m_saveBtn, &QPushButton::clicked, this, &LutTableWidget::saveToFile);

	if(!m_dev) {
		setStatus("Device not present in this context.", true);
		m_readBtn->setEnabled(false);
		m_writeBtn->setEnabled(false);
	} else if(!iio_device_find_attr(m_dev, m_attribute.toStdString().c_str())) {
		setStatus(QString("Attribute \"%1\" is not exposed by this driver.").arg(m_attribute), true);
		m_readBtn->setEnabled(false);
		m_writeBtn->setEnabled(false);
	}
}

LutTableWidget::~LutTableWidget() {}

QString LutTableWidget::text() const { return m_editor->toPlainText(); }

void LutTableWidget::setStatus(const QString &message, bool isError)
{
	m_status->setText(message);
	m_status->setStyleSheet(
		QString("color: %1;")
			.arg(Style::getAttribute(isError ? json::theme::content_error : json::theme::content_success)));
}

void LutTableWidget::readTable()
{
	if(!m_dev) {
		return;
	}

	QVector<char> buffer(LutBufferSize, 0);
	ssize_t ret = iio_device_attr_read(m_dev, m_attribute.toStdString().c_str(), buffer.data(), LutBufferSize);
	if(ret < 0) {
		setStatus(QString("Read failed (%1).").arg(ret), true);
		qWarning(CAT_NEPONSET_LUT) << "Read of" << m_attribute << "failed:" << ret;
		return;
	}

	const QString table = QString::fromUtf8(buffer.data());
	m_editor->setPlainText(table);

	// A table that exactly fills the buffer was probably cut short.
	if(static_cast<size_t>(ret) >= LutBufferSize - 1) {
		setStatus(QString("Read %1 bytes, which fills the buffer - the table may be truncated.").arg(ret),
			  true);
		return;
	}

	const int lines = table.trimmed().isEmpty() ? 0 : table.trimmed().split('\n').count();
	setStatus(QString("Read %1 entries (%2 bytes).").arg(lines).arg(ret), false);
}

void LutTableWidget::writeTable()
{
	if(!m_dev) {
		return;
	}

	const QByteArray data = m_editor->toPlainText().toUtf8();
	if(data.isEmpty()) {
		setStatus("Nothing to write.", true);
		return;
	}

	if(m_ctx) {
		iio_context_set_timeout(m_ctx, LutWriteTimeoutMs);
	}
	ssize_t ret = iio_device_attr_write_raw(m_dev, m_attribute.toStdString().c_str(), data.constData(),
					       data.size());
	if(m_ctx) {
		iio_context_set_timeout(m_ctx, DefaultTimeoutMs);
	}

	if(ret < 0) {
		setStatus(QString("Write failed (%1). The table was not programmed.").arg(ret), true);
		qWarning(CAT_NEPONSET_LUT) << "Write of" << m_attribute << "failed:" << ret;
		return;
	}

	setStatus(QString("Wrote %1 bytes.").arg(data.size()), false);
	Q_EMIT tableWritten(m_attribute);
}

void LutTableWidget::loadFromFile()
{
	const QString path = QFileDialog::getOpenFileName(this, QString("Load %1").arg(m_attribute), QString(),
							  "Table files (*.txt *.lut);;All files (*)");
	if(path.isEmpty()) {
		return;
	}

	QFile file(path);
	if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		setStatus(QString("Could not open %1.").arg(path), true);
		return;
	}

	QTextStream stream(&file);
	m_editor->setPlainText(stream.readAll());
	file.close();

	setStatus(QString("Loaded from %1. Press Write to program it.").arg(QFileInfo(path).fileName()), false);
}

void LutTableWidget::saveToFile()
{
	const QString path = QFileDialog::getSaveFileName(this, QString("Save %1").arg(m_attribute),
							  m_attribute + ".txt",
							  "Table files (*.txt *.lut);;All files (*)");
	if(path.isEmpty()) {
		return;
	}

	QFile file(path);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		setStatus(QString("Could not write %1.").arg(path), true);
		return;
	}

	QTextStream stream(&file);
	stream << m_editor->toPlainText();
	file.close();

	setStatus(QString("Saved to %1.").arg(QFileInfo(path).fileName()), false);
}

#include "moc_luttablewidget.cpp"
