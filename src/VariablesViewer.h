#ifndef SRC_VARIABLESVIEWER_H_
#define SRC_VARIABLESVIEWER_H_

#include <QTableWidget>
#include <QCheckBox>
#include <QGroupBox>
#include <QRadioButton>

class SymbolTable;
class MemoryLayout;
class QTableWidget;
class QCheckbox;
class QGroup;

class VariablesViewer : public QTableWidget
{
	Q_OBJECT;
public:
	VariablesViewer(QWidget* parent = nullptr);

	void setSymbolTable(SymbolTable* st);
	void setMemoryLayout(MemoryLayout* ml);

	void initTable();
	void updateTable();
	void updateVariableValue(int variableId, QString value, int column);
	void updateVariableValue8(int variableId, QString value);
	void updateVariableValue16(int variableId, QString value);


public slots:
	void symbolsChanged();
	void symbolFileChanged();
	void valueFormatChanged(bool checked);
	void showJumpLabelsChanged(int state);

private:
	void resizeEvent(QResizeEvent* e) override;
	void updateVariablesValueFormat();

	bool showJumpLabels;
	QString valueFormat;
	SymbolTable* symTable;
	MemoryLayout* memLayout;
	QTableWidget* variablesTable;
	QCheckBox* showJumpLabelsCheckBox;
	QGroupBox* valueFormatGroup;
	QRadioButton* radioDecimal;
	QRadioButton* radioHex;
};

#endif /* SRC_VARIABLESVIEWER_H_ */
