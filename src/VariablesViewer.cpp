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
#include <QHBoxLayout>
#include <QCheckBox>
#include <QRadioButton>
#include <QGroupBox>

/*
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

	auto* hbox = new QHBoxLayout();
	hbox->setMargin(0);

	showJumpLabels = true;
	valueFormat = "decimal";

	showJumpLabelsCheckBox = new QCheckBox("Show jump labels");
	valueFormatGroup = new QGroupBox("Value format");
	radioDecimal = new QRadioButton("dec");
	radioHex = new QRadioButton("hex");

	radioDecimal->setChecked(true);
	showJumpLabelsCheckBox->setChecked(true);

	auto* hboxValueButtons = new QHBoxLayout();
	hboxValueButtons->setMargin(0);
	hboxValueButtons->addWidget(radioDecimal);
	hboxValueButtons->addWidget(radioHex);
	hboxValueButtons->addStretch(1);
	valueFormatGroup->setLayout(hboxValueButtons);

	hbox->addWidget(showJumpLabelsCheckBox);
	hbox->addWidget(valueFormatGroup);

	auto* vbox = new QVBoxLayout();
	vbox->setMargin(0);
	vbox->addLayout(hbox);
	vbox->addWidget(variablesTable);
	setLayout(vbox);

	connect(showJumpLabelsCheckBox, SIGNAL(stateChanged(int)),
	        this, SLOT(showJumpLabelsChanged(int)));
	connect(radioDecimal, SIGNAL(toggled(bool)),
			this, SLOT(valueFormatChanged(bool)));

	initTable();
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
	int rowCount = 0;
	if (symTable != nullptr){
		QStringList variables;
		Symbol* symbol;
		QString symbolType;
		variables = symTable->labelList(true, memLayout);
		for (int i = 0; i < variables.size(); i += 1) {
			symbol = symTable->getAddressSymbol(variables[i], true);
			if ( (symbol->type() == 0 && showJumpLabels)
					|| symbol->type() != 0) {
				variablesTable->setRowCount(rowCount+1);
				QTableWidgetItem* variable =
						new QTableWidgetItem(variables[i], 0);
				variablesTable->setItem(rowCount, 0, variable);
				switch(symbol->type()){
					case 0: symbolType = QString("Jump label");
							break;
					case 1: symbolType = QString("Variable label");
							break;
					default:
							symbolType = QString("Unknown");
				}
				QTableWidgetItem* symbolTypeItem =
						new QTableWidgetItem(symbolType, 0);
				variablesTable->setItem(rowCount, 1, symbolTypeItem);
				QTableWidgetItem* address = new QTableWidgetItem(
						QString("$%1").arg(symbol->value(), 4, 16,
								QLatin1Char('0')));
				variablesTable->setItem(rowCount, 2, address);
				rowCount++;
			}
		}
	}
}

void VariablesViewer::valueFormatChanged(bool checked)
{
	if (checked) {
		valueFormat = QString("decimal");
	} else {
		valueFormat = QString("hex");
	}
	updateVariablesValueFormat();
}

void VariablesViewer::showJumpLabelsChanged(int state){
	if ( state == Qt::CheckState::Checked ){
		showJumpLabels = true;
	} else {
		showJumpLabels = false;
	}
	// need to turn sorting off and on (otherwise the app will crash)
	variablesTable->setSortingEnabled(false);
	initTable();
	variablesTable->setSortingEnabled(true);
	updateTable();
}

void VariablesViewer::symbolsChanged()
{
	// need to turn sorting off and on (otherwise the app will crash)
	variablesTable->setSortingEnabled(false);
	initTable();
	variablesTable->setSortingEnabled(true);
	updateTable();
}

void VariablesViewer::symbolFileChanged()
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


void VariablesViewer::updateVariablesValueFormat()
{
	QTableWidgetItem* Item8;
	QTableWidgetItem* Item16;
	int value8, value16;
	QString valueStr8, valueStr16;
	bool ok;
	for (int i = 0; i < variablesTable->rowCount(); i += 1) {
		Item8 = variablesTable->item(i, 3);
		Item16 = variablesTable->item(i, 4);
		if (valueFormat == "decimal"){
			// old values were in hex format prefixed with $
			value8 = Item8->text().remove(0,1).toInt(&ok, 16);
			value16 = Item16->text().remove(0,1).toInt(&ok, 16);
			valueStr8 = QString("%1").arg(value8);
			valueStr16 = QString("%1").arg(value16);
		} else {
			// old values were in decimal format
			value8 = Item8->text().toInt(&ok, 10);
			value16 = Item16->text().toInt(&ok, 10);
			valueStr8 = QString("$%1").arg(value8, 2, 16, QLatin1Char('0'));
			valueStr16 = QString("$%1").arg(value16, 4, 16, QLatin1Char('0'));
		}
		Item8->setText(valueStr8);
		Item16->setText(valueStr16);
	}
}

void VariablesViewer::updateVariableValue(int variableId, QString value,
		int column)
{
	QString oldValue;
	QTableWidgetItem* oldItem;
	QBrush textColor = Qt::black;
	bool ok;
	int oldVal, newVal;
	newVal = value.toInt(&ok, 10);
	oldItem = variablesTable->item(variableId, column);
	if (oldItem != nullptr) {
		oldValue = oldItem->text();
		if (valueFormat == "hex"){
			oldValue.remove(0,1);
			oldVal = oldValue.toInt(&ok, 16);
		} else {
			oldVal = oldValue.toInt(&ok, 10);
		}
		if ( newVal != oldVal ){
			textColor = Qt::red;
		}
	}
	if (valueFormat == "hex"){
		if (newVal <= 255) {
			value = QString("$%1").arg(newVal, 2, 16, QLatin1Char('0'));
		} else {
			value = QString("$%1").arg(newVal, 4, 16, QLatin1Char('0'));
		}
	} else {
		value = QString("%1").arg(newVal);
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
