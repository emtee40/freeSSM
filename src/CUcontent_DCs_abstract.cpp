/*
 * CUcontent_DCs_abstract.cpp - Abstract widget for Diagnostic Codes Reading
 *
 * Copyright (C) 2008-2009 Comer352l
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CUcontent_DCs_abstract.h"


CUcontent_DCs_abstract::CUcontent_DCs_abstract(QWidget *parent, SSMprotocol2 *SSMP2dev, QString progversion) : QWidget(parent)
{
	_SSMP2dev = SSMP2dev;
	_progversion = progversion;
	_supportedDCgroups = 0;
}


CUcontent_DCs_abstract::~CUcontent_DCs_abstract()
{
	disconnect(_SSMP2dev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() )); // this must be done BEFORE calling _SSMP2dev->stopDCreading() !
	_SSMP2dev->stopDCreading();
	disconnect(_SSMP2dev, SIGNAL( startedDCreading() ), this, SLOT( callStart() ));
	disconnect(_SSMP2dev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() ));
}


void CUcontent_DCs_abstract::callStart()
{
	if (!startDCreading())
		communicationError(tr("Couldn't start Diagnostic Codes Reading."));
}


void CUcontent_DCs_abstract::callStop()
{
	if (!stopDCreading())
		communicationError(tr("Couldn't stop Diagnostic Codes Reading."));
}


bool CUcontent_DCs_abstract::startDCreading()
{
	SSMprotocol2::state_dt state = SSMprotocol2::state_needSetup;
	int selDCgroups = 0;
	// Check if DC-group(s) selected:
	if (_supportedDCgroups == SSMprotocol2::noDCs_DCgroup)
		return false;
	// Check if DC-reading is startable or already in progress:
	state = _SSMP2dev->state();
	if (state == SSMprotocol2::state_normal)
	{
		disconnect(_SSMP2dev, SIGNAL( startedDCreading() ), this, SLOT( callStart() ));
		// Start DC-reading:
		if (!_SSMP2dev->startDCreading( _supportedDCgroups ))
		{
			connect(_SSMP2dev, SIGNAL( startedDCreading() ), this, SLOT( callStart() ));
			return false;
		}
	}
	else if (state == SSMprotocol2::state_DCreading)
	{
		// Verify consistency:
		if (!_SSMP2dev->getLastDCgroupsSelection(&selDCgroups))
			return false;
		if (selDCgroups != _supportedDCgroups)  // inconsistency detected !
		{
			// Stop DC-reading:
			stopDCreading();
			return false;
		}
	}
	else
		return false;
	// Enable notification about external DC-reading-stops:
	connect(_SSMP2dev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() ));
	// Connect DC-table and print-button slots:
	connectGUIelements();
	return true;
}


bool CUcontent_DCs_abstract::stopDCreading()
{
	disconnect(_SSMP2dev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() )); // this must be done BEFORE calling _SSMP2dev->stopDCreading() !
	if (_SSMP2dev->state() == SSMprotocol2::state_DCreading)
	{
		if (!_SSMP2dev->stopDCreading())
		{
			connect(_SSMP2dev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() )); // this must be done BEFORE calling _SSMP2dev->stopDCreading() !
			return false;
		}
	}
	connect(_SSMP2dev, SIGNAL( startedDCreading() ), this, SLOT( callStart() ));
	disconnectGUIelements();
	return true;
}


void CUcontent_DCs_abstract::setDCtableContent(QTableWidget *tableWidget, QStringList DCs, QStringList DCdescriptions)
{
	int k = 0;
	QTableWidgetItem *tableelement;
	// Delete table content:
	tableWidget->clearContents();
	// Set number of rows:
	setNrOfTableRows(tableWidget, DCdescriptions.size());
	// Fill Table:
	for (k=0; k<DCs.size(); k++)
	{
		// Output DC:
		tableelement = new QTableWidgetItem(DCs.at(k));
		tableelement->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		tableWidget->setItem(k, 0, tableelement);
		// Output DC description:
		tableelement = new QTableWidgetItem(DCdescriptions.at(k));
		tableWidget->setItem(k, 1, tableelement);
	}
}


void CUcontent_DCs_abstract::setNrOfTableRows(QTableWidget *tablewidget, unsigned int nrofUsedRows)
{
	// NOTE: this function doesn't change the table's content ! Min. 1 row !
	int rowheight = 0;
	int vspace = 0;
	QHeaderView *headerview;
	unsigned int minnrofrows = 0;
	// Get available vertical space (for rows) and height per row:
	if (tablewidget->rowCount() < 1)
		tablewidget->setRowCount(1); // temporary create a row to get the row hight
	rowheight = tablewidget->rowHeight(0);
	headerview = tablewidget->horizontalHeader();
	vspace = tablewidget->viewport()->height();
	// Temporary switch to "Scroll per Pixel"-mode to ensure auto-scroll (prevent white space between bottom of the last row and the lower table border)
	tablewidget->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
	// Calculate and set nr. of rows:
	minnrofrows = static_cast<unsigned int>(trunc((vspace-1)/rowheight) + 1);
	if (minnrofrows < nrofUsedRows)
		minnrofrows = nrofUsedRows;
	tablewidget->setRowCount(minnrofrows);
	// Set vertical scroll bar policy:
	if (minnrofrows > nrofUsedRows)
		tablewidget->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	else
		tablewidget->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	// Switch back to "Scroll per Item"-mode:
	tablewidget->setVerticalScrollMode( QAbstractItemView::ScrollPerItem ); // auto-scroll is triggered; Maybe this is a Qt-Bug, we don't want that here...
}


void CUcontent_DCs_abstract::printDCprotocol()
{
	QString datetime;
	QString CU;
	QString systype;
	std::string ROM_ID;
	QString VIN = tr("not supported by ECU");
	bool VINsup = false;
	bool ok = false;
	QString errstr = "";

	// Create Printer:
	QPrinter printer(QPrinter::ScreenResolution);
	// Show print dialog:
	QPrintDialog printDialog(&printer, this);
	if (printDialog.exec() != QDialog::Accepted)
		return;
	// Show print status message:
	QMessageBox printmbox( QMessageBox::NoIcon, tr("Printing..."), tr("Printing... Please wait !    "), QMessageBox::NoButton, this);
	printmbox.setStandardButtons(QMessageBox::NoButton);
	QPixmap printicon(QString::fromUtf8(":/icons/chrystal/32x32/fileprint"));
	printmbox.setIconPixmap(printicon);
	QFont printmboxfont = printmbox.font();
	printmboxfont.setPixelSize(13);	// 10pts
	printmboxfont.setBold(true);
	printmbox.setFont( printmboxfont );
	printmbox.show();
	// ##### GATHER CU-INFORMATIONS #####
	datetime = QDateTime::currentDateTime().toString("dddd, dd. MMMM yyyy, h:mm") + ":";
	SSMprotocol2::CUtype_dt cu_type = _SSMP2dev->CUtype();
	if (cu_type == SSMprotocol2::ECU)
	{
		CU = tr("Engine");
	}
	else if (cu_type == SSMprotocol2::TCU)
	{
		CU = tr("Transmission");
	}
	else
	{
		CU = tr("UNKNOWN");
	}
	ok = true;
	if (!_SSMP2dev->getSystemDescription(&systype))
	{
		std::string SYS_ID = "";
		SYS_ID = _SSMP2dev->getSysID();
		ok = SYS_ID.length();
		if (ok)
			systype = tr("Unknown (") + QString::fromStdString(SYS_ID) + ")";	// NOTE: SYS_ID is always available, if CU is initialized/connection is alive
		else
			errstr = tr("Query of the System-ID failed.");
	}
	if (ok)
	{
		ROM_ID = _SSMP2dev->getROMID();		// NOTE: ROM_ID is always available, if CU is initialized/connection is alive
		ok = ROM_ID.length();
		if (ok && (cu_type == SSMprotocol2::ECU))
		{
			ok = _SSMP2dev->hasVINsupport(&VINsup);
			if (ok)
			{
				if ( VINsup )
				{
					// Temporary stop DC-reading for VIN-Query:
					disconnect(_SSMP2dev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() ));
					ok = _SSMP2dev->stopDCreading();
					if (ok)
					{
						// Query VIN:
						ok = _SSMP2dev->getVIN(&VIN);
						if (ok)
						{
							if (VIN.size() == 0)
								VIN = tr("not programmed yet");
							// Restart DC-reading:
							ok = _SSMP2dev->restartDCreading();
							if (ok)
								connect(_SSMP2dev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() ));
							else
								errstr = tr("Couldn't restart Diagnostic Codes Reading.");
						}
						else
							errstr = tr("Query of the VIN failed.");
					}
					else
						errstr = tr("Couldn't stop Diagnostic Codes Reading.");
				}
			}
			else
				errstr = tr("Couldn't determine if VIN-registration is supported.");
		}
		else
			errstr = tr("Query of the ROM-ID failed.");
	}
	// Check for communication error:
	if (!ok)
	{
		printmbox.close();
		communicationError(errstr);
		return;
	}
	// ##### CREATE TEXT DOCUMENT #####
	// Create text document and get cursor position:
	QTextDocument textDocument;
	QTextCursor cursor(&textDocument);
	// Define formats:
	QTextBlockFormat blockFormat;
	QTextCharFormat charFormat;
	QTextTableFormat tableFormat;
	// --- TITLE ---
	// Set title format:
	blockFormat.setAlignment(Qt::AlignHCenter);
	charFormat.setFontPointSize(18);
	charFormat.setFontWeight(QFont::Normal);
	charFormat.setFontUnderline(true);
	// Put title into text document:
	cursor.setBlockFormat(blockFormat);
	cursor.setCharFormat(charFormat);
	cursor.insertText("FreeSSM " + _progversion);
	cursor.insertBlock();
	// --- DATE, TIME ---
	// Format date + time:
	blockFormat.setAlignment(Qt::AlignLeft);
	blockFormat.setTopMargin(15);
	charFormat.setFontPointSize(14);
	charFormat.setFontWeight(QFont::Normal);
	charFormat.setFontUnderline(false);
	cursor.setBlockFormat(blockFormat);
	cursor.setCharFormat(charFormat);
	// Put date + time into the text document:
	cursor.insertText(datetime);
	// ----- CU-INFORMATIONS -----
	// -- Create frame for CU-Informations:
	QTextFrameFormat infoFrame;
	infoFrame.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
	infoFrame.setBorder(1);
	infoFrame.setBorderBrush(QBrush(QColor("black"), Qt::SolidPattern));
	infoFrame.setTopMargin(10);
	cursor.insertFrame(infoFrame);
	// -- TABLE OF CU-INFORMATIONS:
	// Set minimal columns widths:
	QVector<QTextLength> widthconstraints(2);
	widthconstraints[0] = QTextLength(QTextLength::PercentageLength, 50);
	widthconstraints[1] = QTextLength(QTextLength::PercentageLength, 50);
	// Set format:
	tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_None);
	tableFormat.setColumnWidthConstraints(widthconstraints);
	tableFormat.setLeftMargin(20);
	// Create and insert table:
	if (cu_type == SSMprotocol2::ECU)
		cursor.insertTable(4, 2, tableFormat);
	else
		cursor.insertTable(3, 2, tableFormat);
	// Set font format:
	charFormat.setFontPointSize(12);
	charFormat.setFontUnderline(false);
	// Fill table cells:
	// Row 1 - Column 1:
	charFormat.setFontWeight(QFont::Bold);
	cursor.setCharFormat(charFormat);
	cursor.insertText(tr("Control Unit:"));
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 1 - Column 2:
	charFormat.setFontWeight(QFont::Normal);
	cursor.setCharFormat(charFormat);
	cursor.insertText(CU);
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 2 - Column 1:
	charFormat.setFontWeight(QFont::Bold);
	cursor.setCharFormat(charFormat);
	cursor.insertText(tr("System Type:"));
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 2 - Column 2:
	charFormat.setFontWeight(QFont::Normal);
	cursor.setCharFormat(charFormat);
	cursor.insertText(systype);
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 3 - Column 1:
	charFormat.setFontWeight(QFont::Bold);
	cursor.setCharFormat(charFormat);
	cursor.insertText(tr("ROM-ID:"));
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 3 - Column 2:
	charFormat.setFontWeight(QFont::Normal);
	cursor.setCharFormat(charFormat);
	cursor.insertText( QString::fromStdString(ROM_ID) );
	if (cu_type == SSMprotocol2::ECU)
	{
		cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
		// Row 4 - Column 1:
		charFormat.setFontWeight(QFont::Bold);
		cursor.setCharFormat(charFormat);
		cursor.insertText(tr("Registered VIN:"));
		cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
		// Row 4 - Column 2:
		charFormat.setFontWeight(QFont::Normal);
		cursor.setCharFormat(charFormat);
		cursor.insertText(VIN);
	}
	// Move (out of the table) to the next block:
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Move (out of the frame) to the next block:
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Create DC-tables:
	createDCprintTables(cursor);
	// Print created text-document:
	textDocument.print(&printer);
	// Delete status message:
	printmbox.close();
}


void CUcontent_DCs_abstract::insertDCprintTable(QTextCursor cursor, QString title, QStringList codes, QStringList descriptions)
{
	QTextTableFormat tableFormat;
	QTextCharFormat charFormat;
	QVector<QTextLength> widthconstraints(2);
	int k = 0;

	// Insert space line:
	cursor.insertBlock();
	// Set minimal column widths:
	widthconstraints.clear();
	widthconstraints.resize(2);
	widthconstraints[0] = QTextLength(QTextLength::PercentageLength,15);
	widthconstraints[1] = QTextLength(QTextLength::PercentageLength,85);
	// Set table format:
	tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_None);
	tableFormat.setColumnWidthConstraints(widthconstraints);
	tableFormat.setLeftMargin(30);
	// Set title:
	charFormat.setFontPointSize(14);
	charFormat.setFontWeight(QFont::Bold);
	charFormat.setFontUnderline(false);
	cursor.setCharFormat(charFormat);
	cursor.insertText( title );
	// Create and insert table:
	cursor.insertTable(codes.size(), 2, tableFormat); // n rows, 2 columns
	// Set font format for DCs:
	charFormat.setFontPointSize(12);
	charFormat.setFontWeight(QFont::Normal);
	charFormat.setFontUnderline(false);
	// Fill table cells:
	for (k=0; k<codes.size(); k++)
	{
		//   Column 1: Code
		cursor.setCharFormat(charFormat);
		cursor.insertText( codes.at(k) );
		cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
		//   Column 2: Description
		cursor.setCharFormat(charFormat);
		cursor.insertText( descriptions.at(k) );
		cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	}
}


void CUcontent_DCs_abstract::communicationError(QString errstr)
{
	QMessageBox msg( QMessageBox::Critical, tr("Communication Error"), tr("Communication Error:") + ('\n') + errstr, QMessageBox::Ok, this);
	QFont msgfont = msg.font();
	msgfont.setPixelSize(12); // 9pts
	msg.setFont( msgfont );
	msg.show();
	msg.exec();
	msg.close();
	emit error();
}

