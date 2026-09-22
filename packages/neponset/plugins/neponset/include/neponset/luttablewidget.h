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

#ifndef LUTTABLEWIDGET_H
#define LUTTABLEWIDGET_H

#include "scopy-neponset_export.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QWidget>
#include <iio.h>

namespace scopy::neponset {

/**
 * Editor for a multi-line lookup table device attribute.
 *
 * The Neponset converters and filters each carry lookup tables as a single newline-separated
 * attribute: filter_table_config_A/B (32 entries), gain_table_config (67 entries),
 * lut_config_A/B (32 entries) and lut_config (64 entries). No IIOWidget can represent these -
 * IIOWidgetBuilder's UI strategies top out at a single-line MenuLineEdit - so this widget
 * talks to libiio directly, in the same way that the ADRV9002 profile manager and the AD9371
 * profile loader do for their own large blobs.
 *
 * Two details matter and are easy to get wrong:
 *
 *  - reads need a buffer well over libiio's 1 KiB default or the table comes back truncated.
 *    pyadi-iio works around the same limit with its own 8 KiB _read_dev_attr_large helper.
 *  - writes have to go through iio_device_attr_write_raw with an explicit length, because the
 *    value contains newlines, and they need a longer context timeout since programming a full
 *    table is slow.
 */
class SCOPY_NEPONSET_EXPORT LutTableWidget : public QWidget
{
	Q_OBJECT
public:
	LutTableWidget(iio_context *ctx, iio_device *dev, const QString &attribute, const QString &title,
		       QWidget *parent = nullptr);
	~LutTableWidget();

	/** Table text currently in the editor. */
	QString text() const;

public Q_SLOTS:
	/** Reads the attribute into the editor. Safe to call when the device is absent. */
	void readTable();
	/** Writes the editor contents to the attribute. */
	void writeTable();

Q_SIGNALS:
	/** Emitted after a write that the device accepted. */
	void tableWritten(const QString &attribute);

private:
	void loadFromFile();
	void saveToFile();
	void setStatus(const QString &message, bool isError);

	iio_context *m_ctx = nullptr;
	iio_device *m_dev = nullptr;
	QString m_attribute;

	QPlainTextEdit *m_editor = nullptr;
	QPushButton *m_readBtn = nullptr;
	QPushButton *m_writeBtn = nullptr;
	QPushButton *m_loadBtn = nullptr;
	QPushButton *m_saveBtn = nullptr;
	QLabel *m_status = nullptr;
};

} // namespace scopy::neponset

#endif // LUTTABLEWIDGET_H
