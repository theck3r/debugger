/*
 * VariableViewer.cpp
 *
 *  Created on: Feb 13, 2021
 *      Author: thies
 */


#include "VariablesViewer.h"
#include "SymbolTable.h"
#include "CommClient.h"
#include "OpenMSXConnection.h"
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QBrush>
#include <QChar>
#include <QStringList>
#include <QString>
#include <QVBoxLayout>

class DebugAddress8Call : public SimpleCommand
{
public:
	DebugAddress8Call(VariablesViewer& viewer_, int address_, int variable_id_)
		: SimpleCommand(QString("peek %1").arg(address_))
		, viewer(viewer_)
		, address(address_)
		, variable_id(variable_id_)
	{
	}

	void replyOk(const QString& message) override
	{
		viewer.updateVariableValue8(variable_id, message);
		delete this;
	}
private:
	VariablesViewer& viewer;
	int address;
	int variable_id;
};

// call the debugger to return 16bit value at address
class DebugAddress16Call : public SimpleCommand
{
public:
	DebugAddress16Call(VariablesViewer& viewer_, int address_, int variable_id_)
		: SimpleCommand(QString("peek16 %1").arg(address_))
		, viewer(viewer_)
		, address(address_)
		, variable_id(variable_id_)
	{
	}

	void replyOk(const QString& message) override
	{
		viewer.updateVariableValue16(variable_id, message);
		delete this;
	}
private:
	VariablesViewer& viewer;
	int address;
	int variable_id;
};

VariablesViewer::VariablesViewer(QWidget* parent)
	: QTableWidget(parent)
{
	variablesTable = new QTableWidget();
	variablesTable->setColumnCount(5);
	variablesTable->setRowCount(0);
	variablesTable->setAlternatingRowColors(true);
	variablesTable->setSortingEnabled(true);

	// QStringList header_items;
	QStringList header_items = QStringList();
	header_items.append(QString("Label"));
	header_items.append(QString("Type"));
	header_items.append(QString("Address (hex)"));
	header_items.append(QString("Value (8bit)"));
	header_items.append(QString("Value (16bit)"));
	variablesTable->setHorizontalHeaderLabels(header_items);

	auto* vbox = new QVBoxLayout();
	vbox->setMargin(0);
	vbox->addWidget(variablesTable);
	setLayout(vbox);
}

void VariablesViewer::setSymbolTable(SymbolTable* st)
{
	symTable = st;
}

void VariablesViewer::setMemoryLayout(MemoryLayout* ml)
{
	memLayout = ml;
}

void VariablesViewer::initTable()
{
	variablesTable->setRowCount(0);	// clear table first
	QStringList variables;
	Symbol* symbol;
	variables = symTable->labelList(true, memLayout);
	variablesTable->setRowCount(variables.size());
	for (int i = 0; i < variables.size(); i += 1) {
		symbol = symTable->getAddressSymbol(variables[i], true);
		QTableWidgetItem* variable = new QTableWidgetItem(variables[i], 0);
		variablesTable->setItem(i, 0, variable);
		QTableWidgetItem* symbol_type = new QTableWidgetItem(
				QString("%1").arg(symbol->type(), 0));
		variablesTable->setItem(i, 1, symbol_type);
		QTableWidgetItem* address = new QTableWidgetItem(
				QString("$%1").arg(symbol->value(), 4, 16, QChar(48)));
		variablesTable->setItem(i, 2, address);
	}
}

void VariablesViewer::symbolsChanged()
{
	// need to turn sorting off and on (otherwise the app will crash)
	variablesTable->setSortingEnabled(false);
	initTable();
	variablesTable->setSortingEnabled(true);
	updateTable();
}

void VariablesViewer::updateTable()
{
	QString address_text;
	int address;
	bool ok;
	for (int i = 0; i < variablesTable->rowCount(); i += 1) {
		address_text = variablesTable->item(i, 2)->text();
		address = address_text.right(4).toInt(&ok, 16);
		CommClient::instance().sendCommand(new DebugAddress8Call(*this,
				address, i));
		CommClient::instance().sendCommand(new DebugAddress16Call(*this,
				address, i));
	}
}

void VariablesViewer::updateVariableValue(int variable_id, QString value,
		int column)
{
	QString old_value;
	QTableWidgetItem* old_item;
	QBrush text_color = Qt::black;
	old_item = variablesTable->item(variable_id, column);
	if (old_item != nullptr){
		old_value = old_item->text();
		if ( value != old_value){
			text_color = Qt::red;
		}
	}
	QTableWidgetItem* value_item = new QTableWidgetItem(value, 0);
	value_item->setForeground(text_color);
	variablesTable->setItem(variable_id, column, value_item);
}

void VariablesViewer::updateVariableValue8(int variable_id, QString value)
{
	updateVariableValue(variable_id, value, 3);
}

void VariablesViewer::updateVariableValue16(int variable_id, QString value)
{
	updateVariableValue(variable_id, value, 4);
}

void VariablesViewer::resizeEvent(QResizeEvent* e)
{
	QFrame::resizeEvent(e);
}
