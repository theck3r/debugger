// $Id$

#include "DisasmViewer.h"
#include "OpenMSXConnection.h"
#include "CommClient.h"
#include "DebuggerData.h"
#include "Settings.h"
#include <QPaintEvent>
#include <QPainter>
#include <QStyleOptionFocusRect>
#include <QScrollBar>
#include <QWheelEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <cmath>
#include <cassert>

class CommMemoryRequest : public ReadDebugBlockCommand
{
public:
	CommMemoryRequest(unsigned offset_, unsigned size_, unsigned char* target,
	                  DisasmViewer& viewer_)
		: ReadDebugBlockCommand("memory", offset_, size_, target)
		, offset(offset_), size(size_)
		, viewer(viewer_)
	{
	}

	virtual void replyOk(const QString& message)
	{
		copyData(message);
		viewer.memoryUpdated(this);
	}

	virtual void cancel()
	{
		viewer.updateCancelled(this);
	}

	// TODO Refactor: public members are ugly
	unsigned offset;
	unsigned size;
	int address;
	int line;
	int method;

private:
	DisasmViewer& viewer;
};



DisasmViewer::DisasmViewer(QWidget* parent)
	: QFrame(parent)
{
	setFrameStyle(WinPanel | Sunken);
	setFocusPolicy(Qt::StrongFocus);
	setBackgroundRole(QPalette::Base);
	setSizePolicy( QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Preferred ) );
	breakMarker = QPixmap(":/icons/breakpoint.png");
	pcMarker = QPixmap(":/icons/pcarrow.png");
	
	memory = NULL;
	cursorAddr = 0;
	cursorLine = 0;
	programAddr = 0xFFFF;
	waitingForData = false;
	nextRequest = NULL;

	scrollBar = new QScrollBar(Qt::Vertical, this);
	scrollBar->setMinimum(0);
	scrollBar->setMaximum(0xFFFF);
	scrollBar->setSingleStep(0);
	scrollBar->setPageStep(0);

	settingsChanged();

	// manual scrollbar handling routines (real size of the data is not known)
	connect(scrollBar, SIGNAL(actionTriggered(int)), this, SLOT(scrollBarAction(int)));
	connect(scrollBar, SIGNAL(valueChanged(int)), this, SLOT(scrollBarChanged(int)));
}

QSize DisasmViewer::sizeHint() const
{
	return QSize( xMnemArg+xMCode[0], 20*codeFontHeight );
}

void DisasmViewer::resizeEvent(QResizeEvent* e)
{
	QFrame::resizeEvent(e);

	scrollBar->setGeometry(width() - frameR, frameT,
	                       scrollBar->sizeHint().width(),
	                       height() - frameT - frameB);
	
	// reset the address in order to trigger a check on the disasmLines
	setAddress(scrollBar->value());
}

void DisasmViewer::settingsChanged()
{
	Settings& s = Settings::get();

	frameL = frameT = frameB = frameWidth();
	frameR = frameL + scrollBar->sizeHint().width();
	setMinimumHeight( frameT + 5*fontMetrics().height() + frameB );
	// set font sizes
	QFontMetrics lfm( s.font(Settings::LABEL_FONT) );
	QFontMetrics cfm( s.font(Settings::CODE_FONT) );
	labelFontHeight = lfm.height();
	labelFontAscent = lfm.ascent();
	codeFontHeight = cfm.height();
	codeFontAscent = cfm.ascent();

	// calculate layout locations
	int charWidth = cfm.width("0");
	xAddr = frameL + 40;
	xMCode[0] = xAddr     + 6 * charWidth;
	xMCode[1] = xMCode[0] + 3 * charWidth;
	xMCode[2] = xMCode[1] + 3 * charWidth;
	xMCode[3] = xMCode[2] + 3 * charWidth;
	xMnem = xMCode[3] + 4 * charWidth;
	xMnemArg = xMnem  + 7 * charWidth;

	setMinimumSize( xMCode[0], 2*codeFontHeight );
	setMaximumSize( QApplication::desktop()->width(), QApplication::desktop()->height() );

	update();
}

void DisasmViewer::symbolsChanged()
{
	int disasmStart = disasmLines.front().addr;
	int disasmEnd = disasmLines.back().addr + disasmLines.back().numBytes;
	
	CommMemoryRequest* req = new CommMemoryRequest(
		disasmStart, disasmEnd - disasmStart, &memory[disasmStart], *this);
	req->address = disasmLines[disasmTopLine].addr;
	req->line = disasmLines[disasmTopLine].infoLine;
	req->method = TopAlways;
	
	if (waitingForData) {
		if (nextRequest) delete nextRequest;
		nextRequest = req;
	} else {
		waitingForData = true;
		CommClient::instance().sendCommand(req);
	}
}

void DisasmViewer::paintEvent(QPaintEvent* e)
{
	// call parent for drawing the actual frame
	QFrame::paintEvent(e);

	Settings& s = Settings::get();
	
	QPainter p(this);

	// calc and set drawing bounds
	QRect r(e->rect());
	if (r.left() < frameL) r.setLeft(frameL);
	if (r.top()  < frameT) r.setTop(frameT);
	if (r.right()  > width()  - frameR - 1) r.setRight (width()  - frameR - 1);
	if (r.bottom() > height() - frameB - 1) r.setBottom(height() - frameB - 1);
	p.setClipRect(r);

	// draw background
	p.fillRect(frameL + 32, frameT, width() - 32 - frameL - frameR,
	           height() - frameT - frameB, palette().color(QPalette::Base) );
	
	// calculate lines while drawing
	visibleLines = 0;
	int y = frameT;
	
	QString hexStr;
	const DisasmRow* row;
	bool displayDisasm = memory != NULL && isEnabled();

	p.setFont(s.font(Settings::CODE_FONT));
	while( y < height()-frameB ) {
		// fetch the data for this line
		row = displayDisasm
		    ? &disasmLines[disasmTopLine+visibleLines]
		    : &DISABLED_ROW;

		int h, a;
		switch( row->rowType ) {
			case DisasmRow::LABEL:
				h = labelFontHeight;
				a = labelFontAscent;
				break;
			case DisasmRow::INSTRUCTION:
				h = codeFontHeight;
				a = codeFontAscent;
				break;
		}

		bool isCursorLine = (row->addr == cursorAddr && row->infoLine == cursorLine);
		// draw cursor line
		if (isCursorLine) {
			p.fillRect(frameL + 32, y, width() - 32 - frameL - frameR, h,
			           palette().color(QPalette::Highlight));
			if (hasFocus()) {
				QStyleOptionFocusRect so;
				so.backgroundColor = palette().color(QPalette::Highlight);
				so.rect = QRect(frameL + 32, y, width() - 32 - frameL - frameR, h);
				style()->drawPrimitive(QStyle::PE_FrameFocusRect, &so, &p, this);
			}
			p.setPen(palette().color(QPalette::HighlightedText));
		}

		// if there is a label here, draw the label, otherwise code
		if( row->rowType == DisasmRow::LABEL ) {
			// draw label
			hexStr = QString("%1:").arg(row->instr.c_str());
			p.setFont(s.font(Settings::LABEL_FONT));
			if( !isCursorLine )
				p.setPen(s.fontColor(Settings::LABEL_FONT));
			p.drawText(xAddr, y + a, hexStr);
			p.setFont(s.font(Settings::CODE_FONT));
		} else {
			// draw code line
			// default to text pen 
			if( !isCursorLine )
				p.setPen(s.fontColor(Settings::CODE_FONT));

			// draw breakpoint marker
			if (row->infoLine == 0 && breakpoints->isBreakpoint(row->addr)) {
				p.drawPixmap(frameL + 2, y + h / 2 -5, breakMarker);
				if( !(row->addr == cursorAddr && row->infoLine == cursorLine) ) {
					p.fillRect(frameL + 32, y, width() -32 - frameL - frameR, h,
					           Qt::red);
					p.setPen(Qt::white);
				}
			}
			// draw PC marker
			if (row->addr == programAddr && row->infoLine == 0) {
				p.drawPixmap(frameL + 18, y + h / 2 -5, pcMarker);
			}

			// print the address
			hexStr.sprintf("%04X", row->addr);
			p.drawText(xAddr, y + a, hexStr);

			// print 1 to 4 bytes
			for (int j = 0; j < row->numBytes; ++j) {
				hexStr.sprintf("%02X", displayDisasm ? memory[row->addr + j] : 0);
				p.drawText(xMCode[j], y + a, hexStr);
			}

			// print the instruction and arguments
			p.drawText(xMnem,    y + a, row->instr.substr(0,  7).c_str());
			p.drawText(xMnemArg, y + a, row->instr.substr(7, 20).c_str());
		}
		// next line
		y += h;
		visibleLines++;
		// check for end of data
		if ( disasmTopLine+visibleLines == int(disasmLines.size()) ) break;
	}
	partialBottomLine = y > height()-frameB;
	visibleLines -= partialBottomLine;
}

void DisasmViewer::setAddress(quint16 addr, int infoLine, int method)
{
	int line = findDisasmLine(addr, infoLine);
	if (line >= 0) {
		int dt, db;
		switch (method) {
			case Top:
			case Middle:
			case Bottom:
			case Closest:
				dt = db = 10;
				break;
			case TopAlways:
				dt = 10;
				db = visibleLines + 10;
				break;
			case MiddleAlways:
				dt = db = 10 + visibleLines / 2;
				break;
			case BottomAlways:
				dt = 10 + visibleLines;
				db = 10;
				break;
			default:
				assert(false);
				dt = db = 0; // avoid warning
		}
	
		if ((line > dt || disasmLines[0].addr == 0) &&
		    (line < int(disasmLines.size()) - db ||
		     disasmLines.back().addr+disasmLines.back().numBytes == 0x10000))
		{
			// line is within existing disasm'd data. Find where to put it.
			if ( method == Top || method == TopAlways || 
			    (method == Closest && line < disasmTopLine)) {
				// Move line to top
				disasmTopLine = line;
			} else if ( method == Bottom || method == BottomAlways || 
			           (method == Closest && line >= disasmTopLine+visibleLines )) {
				// Move line to bottom
				disasmTopLine = line - visibleLines + 1;
			} else if ( method == MiddleAlways ||
			           (method == Middle && (line < disasmTopLine || line >= disasmTopLine+visibleLines))) {
				// Move line to middle
				disasmTopLine = line - visibleLines / 2;
			}
			if (disasmTopLine < 0) disasmTopLine = 0;
			if (disasmTopLine >= int(disasmLines.size())) disasmTopLine = disasmLines.size() - 1;
			update();
			return;
		}
	}

	if (method == Closest) {
		if (addr < disasmLines[disasmTopLine].addr) {
			method = Top;
		} else {
			method = Bottom;
		}
	}

	// The requested address it outside the pre-disassembled bounds.
	// This means that a new block of memory must be transfered from
	// openMSX and disassembled.

	// determine disasm bounds
	int disasmStart;
	int disasmEnd;
	int extra = 4 * (visibleLines>9 ? visibleLines+partialBottomLine : 10);

	switch (method) {
		case Middle:
		case MiddleAlways:
			disasmStart = addr - 3 * extra / 2;
			disasmEnd   = addr + 3 * extra / 2;
			break;
		case Bottom:
		case BottomAlways:
			disasmStart = addr - 2 * extra;
			disasmEnd   = addr +     extra;
			break;
		default:
			disasmStart = addr -     extra;
			disasmEnd   = addr + 2 * extra;
	}
	if (disasmStart < 0) disasmStart = 0;
	if (disasmEnd > 0xFFFF) disasmEnd = 0xFFFF;
	
	CommMemoryRequest* req = new CommMemoryRequest(
		disasmStart, disasmEnd - disasmStart + 1, &memory[disasmStart], *this);
	req->address = addr;
	req->line = infoLine;
	req->method = method;
	
	if (waitingForData) {
		if (nextRequest) delete nextRequest;
		nextRequest = req;
	} else {
		waitingForData = true;
		CommClient::instance().sendCommand(req);
	}
}

void DisasmViewer::memoryUpdated(CommMemoryRequest* req)
{
	// disassemble the newly received memory
	dasm(memory, req->offset, req->offset+req->size - 1, disasmLines, memLayout, symTable);

	// locate the requested line 
	disasmTopLine = findDisasmLine( req->address, req->line );

	switch (req->method) {
		case Middle:
		case MiddleAlways:
			disasmTopLine -= visibleLines / 2;
			break;
		case Bottom:
		case BottomAlways:
			disasmTopLine -= visibleLines - 1;
			break;
	}
	if (disasmTopLine < 0) disasmTopLine = 0;
	if (disasmTopLine + visibleLines > int(disasmLines.size()) )
		disasmTopLine = disasmLines.size() - disasmTopLine;

	updateCancelled(req);

	// sync the scrollbar with the actual address reached
	if (!nextRequest) {
		// set the slider with without the signal
		scrollBar->setTracking(false);
		scrollBar->setSliderPosition(disasmLines[disasmTopLine].addr);
		scrollBar->setTracking(true);
		// set the line
		setAddress(disasmLines[disasmTopLine].addr, disasmLines[disasmTopLine].infoLine);
	}

	update();
}

void DisasmViewer::updateCancelled(CommMemoryRequest* req)
{
	delete req;
	if (nextRequest) {
		CommClient::instance().sendCommand(nextRequest);
		nextRequest = NULL;
	} else {
		waitingForData = false;
	}
}

quint16 DisasmViewer::cursorAddress() const
{
	return cursorAddr;
}

quint16 DisasmViewer::programCounter() const
{
	return programAddr;
}

void DisasmViewer::setProgramCounter(quint16 pc)
{
	programAddr = pc;
	setAddress(pc, 0, MiddleAlways);
}

int DisasmViewer::findDisasmLine(quint16 lineAddr, int infoLine)
{
	int line = 0;
	while ( line < int(disasmLines.size()) ) {
		if (lineAddr == disasmLines[line].addr)
		{
			if( infoLine == FIRST_INFO_LINE ) {
				return line;
			} else if( infoLine == LAST_INFO_LINE ) {
				while( line < int(disasmLines.size())-1 ) {
					if( disasmLines[line+1].addr != lineAddr ) break;
					line++;
				}
				return line;
			} else {
				while( lineAddr == disasmLines[line].addr ) {
					if( infoLine == disasmLines[line].infoLine )
						return line;
					line++;
					if( line == int(disasmLines.size()) ) break;
				}
			}
			return -1;
		}
		line++;
	}
	return -1;
}

void DisasmViewer::scrollBarAction(int action)
{
	switch (action) {
		case QScrollBar::SliderSingleStepAdd:
			setAddress(disasmLines[disasmTopLine + 1].addr, disasmLines[disasmTopLine + 1].infoLine, TopAlways);
			break;
		case QScrollBar::SliderSingleStepSub:
			if (disasmTopLine > 0) {
				setAddress(disasmLines[disasmTopLine - 1].addr, disasmLines[disasmTopLine - 1].infoLine, TopAlways);
			}
			break;
		case QScrollBar::SliderPageStepAdd:
		{
			int line = disasmTopLine + visibleLines + partialBottomLine - 1;
			if ( line < int(disasmLines.size()) ) {
				setAddress(disasmLines[line].addr, disasmLines[line].infoLine, TopAlways);
			}
			break;
		}
		case QScrollBar::SliderPageStepSub:
			if (disasmTopLine > 0) {
				setAddress(disasmLines[disasmTopLine-1].addr, disasmLines[disasmTopLine-1].infoLine, BottomAlways);
			}
			break;
		case QScrollBar::SliderToMinimum:
			setAddress(0, FIRST_INFO_LINE, TopAlways);
			break;
		case QScrollBar::SliderToMaximum:
			setAddress(0xFFFF, LAST_INFO_LINE, BottomAlways);
			break;
	}
}

// moving the slider is handled separately because
// the SliderMoved action won't catch the extreme values
void DisasmViewer::scrollBarChanged(int value)
{
	setAddress(value, TopAlways);
}

void DisasmViewer::setMemory(unsigned char* memPtr)
{
	memory = memPtr;
	// init disasmLines
	DisasmRow newRow;
	newRow.rowType = DisasmRow::INSTRUCTION;
	newRow.numBytes = 1;
	newRow.infoLine = 0;
	newRow.instr = "nop";
	newRow.instr.resize(18, ' ');
	for (int i = 0; i < 150; ++i) {
		newRow.addr = i;
		disasmLines.push_back(newRow);
	}
	disasmTopLine = 50;
}

void DisasmViewer::setBreakpoints(Breakpoints* bps)
{
	breakpoints = bps;
}

void DisasmViewer::setMemoryLayout(MemoryLayout* ml)
{
	memLayout = ml;
}

void DisasmViewer::setSymbolTable(SymbolTable* st)
{
	symTable = st;
}

void DisasmViewer::keyPressEvent(QKeyEvent* e)
{
	switch (e->key()) {
	case Qt::Key_Up: {
		int line = findDisasmLine(cursorAddr, cursorLine);
		if (line > 0) {
			cursorAddr = disasmLines[line - 1].addr;
			cursorLine = disasmLines[line - 1].infoLine;
		}
		setAddress(cursorAddr, cursorLine, Closest);
		e->accept();
		break;
	}
	case Qt::Key_Down: {
		int line = findDisasmLine(cursorAddr, cursorLine);
		if (line >= 0 && line < int(disasmLines.size()) - 1) {
			cursorAddr = disasmLines[line + 1].addr;
			cursorLine = disasmLines[line + 1].infoLine;
		}
		setAddress(cursorAddr, cursorLine, Closest);
		e->accept();
		break;
	}
	case Qt::Key_PageUp:
		if (disasmTopLine > 0) {
			setAddress(disasmLines[disasmTopLine - 1].addr, disasmLines[disasmTopLine - 1].infoLine, BottomAlways);
		}
		e->accept();
		break;
	case Qt::Key_PageDown:
	{
		int line = disasmTopLine + visibleLines + partialBottomLine - 1;
		if ( line >= int(disasmLines.size())) line = disasmLines.size();
		setAddress(disasmLines[line].addr, disasmLines[line].infoLine, TopAlways);
		e->accept();
		break;
	}
	default:
		QFrame::keyReleaseEvent(e);
	}
}

void DisasmViewer::wheelEvent(QWheelEvent *e)
{
	int line = disasmTopLine - e->delta() / 40;
	if( line < 0 ) line = 0;
	
	if (e->orientation() == Qt::Vertical) {
		setAddress(disasmLines[line].addr, disasmLines[line].infoLine, TopAlways);
		e->accept();
	}
}

void DisasmViewer::mousePressEvent(QMouseEvent* e)
{
	if (!(e->button() == Qt::LeftButton)) {
		QFrame::mousePressEvent(e);
		return;
	}

	if (e->x() >= frameL && e->x() < width()  - frameR &&
	    e->y() >= frameT && e->y() < height() - frameB) 
	{
		int line = lineAtPos( e->pos() );
		if (e->x() > frameL + 32) {
			// check if the line exists
			// (bottom of memory could have an empty line)
			if (line + disasmTopLine < int(disasmLines.size())) {
				cursorAddr = disasmLines[disasmTopLine + line].addr;
				cursorLine = disasmLines[disasmTopLine + line].infoLine;
			} else {
				return;
			}
		
			// scroll if partial line
			if (line == visibleLines) {
				setAddress(disasmLines[disasmTopLine + 1].addr, TopAlways);
			} else {
				update();
			}
		} else if (e->x() < frameL+16) {
			// clicked on the breakpoint area
			if (line + disasmTopLine < int(disasmLines.size())) {
				emit toggleBreakpoint(disasmLines[line + disasmTopLine].addr);
			}
		}
	} 
}

int DisasmViewer::lineAtPos( const QPoint& pos )
{
	int line = -1;
	int y = frameT;
	do {
		line++;
		switch( disasmLines[disasmTopLine+line].rowType ) {
			case DisasmRow::LABEL:
				y += labelFontHeight;
				break;
			case DisasmRow::INSTRUCTION:
				y += codeFontHeight;
				break;
		}
	} while( y < pos.y() );
	
	return line;
}
