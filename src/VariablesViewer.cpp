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

/*
TODO: Fix variables table not showing on reload (requires opening symbol
	manager)
TODO: Add selection options for showing jump labels/variables and hex/dec values
TODO: Check why symbols vanish in manager, when selecting type option "value"
*/

class DebugAddress8Call : public SimpleCommand
{
public:
	DebugAddress8Call(VariablesViewer& viewer_, int address_, int variableId_)
		: SimpleCommand(QString("peek %1").arg(address_))
		, viewer(viewer_)
		, address(address_)
		, variableId(variableId_)
	{
	}

	void replyOk(const QString& message) override
	{
		viewer.updateVariableValue8(variableId, message);
		delete this;
	}
private:
	VariablesViewer& viewer;
	int address;
	int variableId;
};

// call the debugger to return 16bit value at address
class DebugAddress16Call : public SimpleCommand
{
public:
	DebugAddress16Call(VariablesViewer& viewer_, int address_, int variableId_)
		: SimpleCommand(QString("peek16 %1").arg(address_))
		, viewer(viewer_)
		, address(address_)
		, variableId(variableId_)
	{
	}

	void replyOk(const QString& message) override
	{
		viewer.updateVariableValue16(variableId, message);
		delete this;
	}
private:
	VariablesViewer& viewer;
	int address;
	int variableId;
};

VariablesViewer::VariablesViewer(QWidget* parent)
	: QTableWidget(parent)
{
	symTable = nullptr;
	memLayout = nullptr;

	variablesTable = new QTableWidget();
	variablesTable->setColumnCount(5);
	variablesTable->setRowCount(0);
	variablesTable->setAlternatingRowColors(true);
	variablesTable->setSortingEnabled(true);

	QStringList headerItems = QStringList();
	headerItems.append(QString("Label"));
	headerItems.append(QString("Type"));
	headerItems.append(QString("Address"));
	headerItems.append(QString("Value (8bit)"));
	headerItems.append(QString("Value (16bit)"));
	variablesTable->setHorizontalHeaderLabels(headerItems);

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
	QString symbolType;
	variables = symTable->labelList(true, memLayout);
	variablesTable->setRowCount(variables.size());
	for (int i = 0; i < variables.size(); i += 1) {
		symbol = symTable->getAddressSymbol(variables[i], true);
		QTableWidgetItem* variable = new QTableWidgetItem(variables[i], 0);
		variablesTable->setItem(i, 0, variable);
		switch(symbol->type()){
			case 0: symbolType = QString("Jump label");
					break;
			case 1: symbolType = QString("Variable label");
					break;
			default:
					symbolType = QString("Unknown");
		}
		QTableWidgetItem* symbolTypeItem = new QTableWidgetItem(symbolType, 0);
		variablesTable->setItem(i, 1, symbolTypeItem);
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
	QString addressText;
	int address;
	bool ok;
	for (int i = 0; i < variablesTable->rowCount(); i += 1) {
		addressText = variablesTable->item(i, 2)->text();
		address = addressText.right(4).toInt(&ok, 16);
		CommClient::instance().sendCommand(new DebugAddress8Call(*this,
				address, i));
		CommClient::instance().sendCommand(new DebugAddress16Call(*this,
				address, i));
	}
}

void VariablesViewer::updateVariableValue(int variableId, QString value,
		int column)
{
	QString oldValue;
	QTableWidgetItem* oldItem;
	QBrush textColor = Qt::black;
	oldItem = variablesTable->item(variableId, column);
	if (oldItem != nullptr){
		oldValue = oldItem->text();
		if ( value != oldValue){
			textColor = Qt::red;
		}
	}
	QTableWidgetItem* valueItem = new QTableWidgetItem(value, 0);
	valueItem->setForeground(textColor);
	variablesTable->setItem(variableId, column, valueItem);
}

void VariablesViewer::updateVariableValue8(int variableId, QString value)
{
	updateVariableValue(variableId, value, 3);
}

void VariablesViewer::updateVariableValue16(int variableId, QString value)
{
	updateVariableValue(variableId, value, 4);
}

void VariablesViewer::resizeEvent(QResizeEvent* e)
{
	QFrame::resizeEvent(e);
}
