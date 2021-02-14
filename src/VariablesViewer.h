/*
 * VariablesViewer.h
 *
 *  Created on: Feb 13, 2021
 *      Author: thies
 */

#ifndef SRC_VARIABLESVIEWER_H_
#define SRC_VARIABLESVIEWER_H_

#include <QTableWidget>

class SymbolTable;
class MemoryLayout;
class QTableWidget;

class VariablesViewer : public QTableWidget
{
	Q_OBJECT;
public:
	VariablesViewer(QWidget* parent = nullptr);

	void setSymbolTable(SymbolTable* st);
	void setMemoryLayout(MemoryLayout* ml);

	void initTable();
	void updateTable();
	void updateVariableValue(int variable_id, QString value);
	void updateVariableValue16(int variable_id, QString value);

public slots:
	void symbolsChanged();

private:
	void resizeEvent(QResizeEvent* e) override;

	SymbolTable* symTable;
	MemoryLayout* memLayout;
	QTableWidget* variablesTable;
};

#endif /* SRC_VARIABLESVIEWER_H_ */
