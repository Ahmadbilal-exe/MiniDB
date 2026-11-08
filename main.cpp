#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTableWidget>
#include <QHeaderView>
#include <QSplitter>
#include <QStatusBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QTableWidgetItem>
#include <QAbstractItemView>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #10141D;
            color: #E5E7EB;
            font-family: Segoe UI;
            font-size: 13px;
        }

        QLabel {
            color: #E5E7EB;
        }

        QListWidget, QTableWidget {
            background-color: #191F2B;
            border: 1px solid #2C3545;
            border-radius: 8px;
            gridline-color: #2C3545;
        }

        QHeaderView::section {
            background-color: #222B3A;
            padding: 10px;
            border: none;
            font-weight: bold;
        }

        QPushButton {
            background-color: #263449;
            border: 1px solid #394B63;
            border-radius: 7px;
            padding: 9px 15px;
        }

        QPushButton:hover {
            background-color: #345477;
        }

        QPushButton:pressed {
            background-color: #1D3854;
        }

        QStatusBar {
            background-color: #191F2B;
        }
    )");

    QMainWindow window;
    window.setWindowTitle("MiniDB - Database Manager");
    window.resize(1100, 700);

    // DATABASE STORAGE LOCATION

    QString projectPath =
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath("data");

    QDir dataDir;

    if (!dataDir.mkpath(projectPath)) {
        QMessageBox::critical(
            nullptr,
            "Storage Error",
            "Cannot create data directory:\n" + projectPath
            );
        return 1;
    }

    qDebug() << "MiniDB storage location:" << projectPath;

    // MAIN LAYOUT

    QWidget *central = new QWidget;
    QHBoxLayout *mainLayout = new QHBoxLayout(central);

    QSplitter *splitter = new QSplitter;

    // SIDEBAR

    QWidget *sidebar = new QWidget;
    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);

    QLabel *logo = new QLabel("MiniDB");

    logo->setStyleSheet(
        "font-size: 25px; font-weight: bold; padding: 12px;"
        );

    QPushButton *newDB = new QPushButton("+ New Database");

    QListWidget *databaseList = new QListWidget;
    QListWidget *tableList = new QListWidget;

    // LOAD EXISTING DATABASES

    QDir folder(projectPath);

    QStringList databases = folder.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot
        );

    for (const QString &db : databases) {
        databaseList->addItem(db);
    }

    sideLayout->addWidget(logo);
    sideLayout->addWidget(newDB);
    sideLayout->addWidget(new QLabel("DATABASES"));
    sideLayout->addWidget(databaseList);
    sideLayout->addWidget(new QLabel("TABLES"));
    sideLayout->addWidget(tableList);

    // CREATE DATABASE

    QObject::connect(newDB, &QPushButton::clicked, [&]() {

        bool ok;

        QString name = QInputDialog::getText(
            &window,
            "Create Database",
            "Enter database name:",
            QLineEdit::Normal,
            "",
            &ok
            );

        name = name.trimmed();

        if (!ok)
            return;

        if (name.isEmpty()) {
            QMessageBox::warning(
                &window,
                "Error",
                "Database name cannot be empty."
                );
            return;
        }

        QRegularExpression validName("^[A-Za-z0-9_-]+$");

        if (!validName.match(name).hasMatch()) {
            QMessageBox::warning(
                &window,
                "Invalid Name",
                "Use only letters, numbers, underscores and hyphens."
                );
            return;
        }

        bool exists = false;

        for (int i = 0; i < databaseList->count(); i++) {
            if (databaseList->item(i)->text().compare(
                    name, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }

        if (exists) {
            QMessageBox::warning(
                &window,
                "Error",
                "Database already exists."
                );
            return;
        }

        QDir databaseDir(projectPath);

        if (!databaseDir.mkdir(name)) {
            QMessageBox::critical(
                &window,
                "Creation Failed",
                "Could not create database folder:\n" +
                    databaseDir.filePath(name)
                );
            return;
        }

        databaseList->addItem(name);
        databaseList->setCurrentRow(databaseList->count() - 1);

        window.statusBar()->showMessage(
            "Database created: " + name
            );

        QMessageBox::information(
            &window,
            "Success",
            "Database created successfully!\n\nLocation:\n" +
                databaseDir.filePath(name)
            );
    });

    // WORKSPACE

    QWidget *workspace = new QWidget;
    QVBoxLayout *workLayout = new QVBoxLayout(workspace);

    QLabel *heading = new QLabel("No Database Selected");

    heading->setStyleSheet(
        "font-size: 23px; font-weight: bold; padding: 10px;"
        );

    QLabel *tableName = new QLabel("No Table Selected");

    QTableWidget *table = new QTableWidget(0, 0);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);

    table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch
        );

    // BUTTONS

    QPushButton *createTable = new QPushButton("+ Create Table");
    QPushButton *insert = new QPushButton("Insert");
    QPushButton *update = new QPushButton("Update");
    QPushButton *remove = new QPushButton("Delete");
    QPushButton *search = new QPushButton("Search All");
    QPushButton *searchColumn = new QPushButton("Search Column");
    QPushButton *sort = new QPushButton("Sort");

    QHBoxLayout *actions = new QHBoxLayout;

    actions->addWidget(createTable);
    actions->addWidget(insert);
    actions->addWidget(update);
    actions->addWidget(remove);
    actions->addWidget(search);
    actions->addWidget(searchColumn);
    actions->addWidget(sort);

    workLayout->addWidget(heading);
    workLayout->addWidget(tableName);
    workLayout->addWidget(table);
    workLayout->addLayout(actions);

    // DATABASE SELECTION AND TABLE DISCOVERY

    QObject::connect(
        databaseList,
        &QListWidget::currentTextChanged,
        [&](const QString &name) {

            if (name.isEmpty())
                return;

            heading->setText(name);

            tableList->clear();

            table->clear();
            table->setRowCount(0);
            table->setColumnCount(0);

            tableName->setText("No Table Selected");

            // FIND CSV FILES IN SELECTED DATABASE

            QDir dbFolder(
                QDir(projectPath).filePath(name)
                );

            QStringList files = dbFolder.entryList(
                QStringList() << "*.csv",
                QDir::Files
                );

            for (const QString &file : files) {

                QString tableNameWithoutExtension = file;
                tableNameWithoutExtension.chop(4);

                tableList->addItem(tableNameWithoutExtension);
            }

            window.statusBar()->showMessage(
                "Selected database: " + name
                );
        }
        );

    // TABLE SELECTION AND CSV LOADING

    QObject::connect(
        tableList,
        &QListWidget::currentTextChanged,
        [&](const QString &name) {

            if (name.isEmpty())
                return;

            QListWidgetItem *selectedDB =
                databaseList->currentItem();

            if (!selectedDB)
                return;

            QString dbName = selectedDB->text();

            QString filePath =
                QDir(projectPath).filePath(
                    dbName + "/" + name + ".csv"
                    );

            QFile file(filePath);

            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::critical(
                    &window,
                    "File Error",
                    "Could not open table file."
                    );
                return;
            }

            QTextStream in(&file);

            // READ COLUMN HEADERS

            if (in.atEnd()) {
                file.close();

                QMessageBox::warning(
                    &window,
                    "Empty Table",
                    "The CSV file has no header row."
                    );
                return;
            }

            QString headerLine = in.readLine();

            QStringList columns = headerLine.split(',');

            table->clear();
            table->setRowCount(0);
            table->setColumnCount(columns.size());
            table->setHorizontalHeaderLabels(columns);

            // READ TABLE RECORDS

            while (!in.atEnd()) {

                QString line = in.readLine();

                if (line.isEmpty())
                    continue;

                QStringList values = line.split(',');

                int row = table->rowCount();

                table->insertRow(row);

                for (int col = 0;
                     col < columns.size() && col < values.size();
                     col++) {

                    table->setItem(
                        row,
                        col,
                        new QTableWidgetItem(values[col])
                        );
                }
            }

            file.close();

            tableName->setText("Table: " + name);

            window.statusBar()->showMessage(
                "Loaded table: " + name
                );
        }
        );

    // SELECT FIRST DATABASE AUTOMATICALLY

    if (databaseList->count() > 0) {
        databaseList->setCurrentRow(0);
    }
    else {
        heading->setText("No Database Selected");
    }

    // CREATE TABLE

    QObject::connect(createTable, &QPushButton::clicked, [&]() {

        QListWidgetItem *selected = databaseList->currentItem();

        if (!selected) {
            QMessageBox::warning(
                &window,
                "Error",
                "Please select a database first."
                );
            return;
        }

        QString dbName = selected->text();

        bool ok;

        QString name = QInputDialog::getText(
            &window,
            "Create Table",
            "Enter table name:",
            QLineEdit::Normal,
            "",
            &ok
            );

        if (!ok)
            return;

        name = name.trimmed();

        QRegularExpression validName("^[A-Za-z0-9_-]+$");

        if (!validName.match(name).hasMatch()) {
            QMessageBox::warning(
                &window,
                "Invalid Name",
                "Use only letters, numbers, underscores and hyphens."
                );
            return;
        }

        // CHECK EXISTING TABLE NAMES

        for (int i = 0; i < tableList->count(); i++) {

            if (tableList->item(i)->text().compare(
                    name, Qt::CaseInsensitive) == 0) {

                QMessageBox::warning(
                    &window,
                    "Error",
                    "Table already exists."
                    );
                return;
            }
        }

        QString columnsInput = QInputDialog::getText(
            &window,
            "Table Columns",
            "Enter column names separated by commas:\nExample: id,name,gpa",
            QLineEdit::Normal,
            "",
            &ok
            );

        if (!ok)
            return;

        QStringList columns = columnsInput.split(',');

        QSet<QString> usedColumns;

        for (QString &column : columns) {

            column = column.trimmed();

            if (!validName.match(column).hasMatch()) {
                QMessageBox::warning(
                    &window,
                    "Invalid Column",
                    "Column names must contain only letters, numbers, underscores and hyphens."
                    );
                return;
            }

            QString key = column.toLower();

            if (usedColumns.contains(key)) {
                QMessageBox::warning(
                    &window,
                    "Duplicate Column",
                    "Column names must be unique."
                    );
                return;
            }

            usedColumns.insert(key);
        }

        QString filePath =
            QDir(projectPath).filePath(
                dbName + "/" + name + ".csv"
                );

        if (QFile::exists(filePath)) {
            QMessageBox::warning(
                &window,
                "Error",
                "Table already exists."
                );
            return;
        }

        QFile file(filePath);

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(
                &window,
                "Error",
                "Could not create CSV file."
                );
            return;
        }

        QTextStream out(&file);

        out << columns.join(',') << '\n';

        file.close();

        // ADD TABLE TO SIDEBAR

        tableList->addItem(name);

        // AUTOMATICALLY OPEN NEW TABLE

        tableList->setCurrentRow(
            tableList->count() - 1
            );

        window.statusBar()->showMessage(
            "Table created: " + name
            );

        QMessageBox::information(
            &window,
            "Success",
            "Table created successfully!\n\nFile:\n" + filePath
            );
    });


    // INSERT RECORD

    QObject::connect(insert, &QPushButton::clicked, [&]() {

        QListWidgetItem *selectedDB = databaseList->currentItem();
        QListWidgetItem *selectedTable = tableList->currentItem();

        if (!selectedDB || !selectedTable) {
            QMessageBox::warning(
                &window,
                "No Table Selected",
                "Please select a database and table first."
                );
            return;
        }

        QString dbName = selectedDB->text();
        QString tblName = selectedTable->text();

        QString filePath =
            QDir(projectPath).filePath(
                dbName + "/" + tblName + ".csv"
                );

        // READ COLUMN NAMES

        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(
                &window,
                "File Error",
                "Could not open the table file."
                );
            return;
        }

        QTextStream in(&file);

        if (in.atEnd()) {
            file.close();

            QMessageBox::warning(
                &window,
                "Empty Table",
                "The table does not contain column headers."
                );
            return;
        }

        QString headerLine = in.readLine();
        file.close();

        if (headerLine.trimmed().isEmpty()) {
            QMessageBox::warning(
                &window,
                "Invalid Table",
                "The table has an empty header row."
                );
            return;
        }

        QStringList columns = headerLine.split(',');

        // ASK FOR EACH COLUMN VALUE

        QStringList values;

        for (const QString &column : columns) {

            bool ok;

            QString value = QInputDialog::getText(
                &window,
                "Insert Record",
                "Enter value for " + column + ":",
                QLineEdit::Normal,
                "",
                &ok
                );

            if (!ok)
                return;

            // This simple CSV version does not support commas or new lines in values.

            if (value.contains(',') ||
                value.contains('\n') ||
                value.contains('\r')) {

                QMessageBox::warning(
                    &window,
                    "Invalid Value",
                    "Values cannot contain commas or new lines."
                    );
                return;
            }

            values.append(value);
        }

        // SAVE RECORD TO CSV

        if (!file.open(QIODevice::WriteOnly |
                       QIODevice::Append |
                       QIODevice::Text)) {

            QMessageBox::critical(
                &window,
                "Save Error",
                "Could not save the new record."
                );
            return;
        }

        QTextStream out(&file);
        out << values.join(',') << '\n';
        file.close();

        // DISPLAY NEW RECORD

        int row = table->rowCount();
        table->insertRow(row);

        for (int col = 0; col < values.size(); col++) {
            table->setItem(
                row,
                col,
                new QTableWidgetItem(values[col])
                );
        }

        table->setCurrentCell(row, 0);

        window.statusBar()->showMessage(
            "Record inserted into " + tblName
            );

        QMessageBox::information(
            &window,
            "Success",
            "Record inserted successfully!"
            );
    });


    // UPDATE RECORD

    QObject::connect(update, &QPushButton::clicked, [&]() {

        QListWidgetItem *selectedDB = databaseList->currentItem();
        QListWidgetItem *selectedTable = tableList->currentItem();

        if (!selectedDB || !selectedTable) {
            QMessageBox::warning(
                &window,
                "No Table Selected",
                "Please select a database and table first."
                );
            return;
        }

        int selectedRow = table->currentRow();

        if (selectedRow < 0) {
            QMessageBox::warning(
                &window,
                "No Record Selected",
                "Please click a record in the table first."
                );
            return;
        }

        QString dbName = selectedDB->text();
        QString tblName = selectedTable->text();

        QString filePath =
            QDir(projectPath).filePath(
                dbName + "/" + tblName + ".csv"
                );

        // READ COLUMN HEADERS

        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(
                &window,
                "File Error",
                "Could not open the table file."
                );
            return;
        }

        QTextStream in(&file);

        if (in.atEnd()) {
            file.close();

            QMessageBox::warning(
                &window,
                "Empty Table",
                "The table does not contain column headers."
                );
            return;
        }

        QString headerLine = in.readLine();
        file.close();

        QStringList columns = headerLine.split(',');

        if (columns.isEmpty() || columns[0].trimmed().isEmpty()) {
            QMessageBox::warning(
                &window,
                "Invalid Table",
                "The table has invalid column headers."
                );
            return;
        }

        // ASK FOR UPDATED VALUES

        QStringList updatedValues;

        for (int col = 0; col < columns.size(); col++) {

            QString oldValue;

            if (table->item(selectedRow, col))
                oldValue = table->item(selectedRow, col)->text();

            bool ok;

            QString value = QInputDialog::getText(
                &window,
                "Update Record",
                "Enter new value for " + columns[col] + ":",
                QLineEdit::Normal,
                oldValue,
                &ok
                );

            if (!ok)
                return;

            // This simple CSV version does not support commas or new lines.

            if (value.contains(',') ||
                value.contains('\n') ||
                value.contains('\r')) {

                QMessageBox::warning(
                    &window,
                    "Invalid Value",
                    "Values cannot contain commas or new lines."
                    );
                return;
            }

            updatedValues.append(value);
        }

        // REWRITE CSV FILE WITH THE UPDATED RECORD

        if (!file.open(QIODevice::WriteOnly |
                       QIODevice::Truncate |
                       QIODevice::Text)) {

            QMessageBox::critical(
                &window,
                "Save Error",
                "Could not save the updated record."
                );
            return;
        }

        QTextStream out(&file);
        out << headerLine << '\n';

        for (int row = 0; row < table->rowCount(); row++) {

            QStringList values;

            for (int col = 0; col < columns.size(); col++) {

                QString value;

                if (row == selectedRow) {
                    value = updatedValues[col];
                }
                else if (table->item(row, col)) {
                    value = table->item(row, col)->text();
                }

                values.append(value);
            }

            out << values.join(',') << '\n';
        }

        file.close();

        // UPDATE THE TABLE VIEW

        for (int col = 0; col < updatedValues.size(); col++) {
            table->setItem(
                selectedRow,
                col,
                new QTableWidgetItem(updatedValues[col])
                );
        }

        table->selectRow(selectedRow);

        window.statusBar()->showMessage(
            "Record updated in " + tblName
            );

        QMessageBox::information(
            &window,
            "Success",
            "Record updated successfully!"
            );
    });


    // DELETE RECORD

    QObject::connect(remove, &QPushButton::clicked, [&]() {

        QListWidgetItem *selectedDB = databaseList->currentItem();
        QListWidgetItem *selectedTable = tableList->currentItem();

        if (!selectedDB || !selectedTable) {
            QMessageBox::warning(
                &window,
                "No Table Selected",
                "Please select a database and table first."
                );
            return;
        }

        int selectedRow = table->currentRow();

        if (selectedRow < 0) {
            QMessageBox::warning(
                &window,
                "No Record Selected",
                "Please select a record to delete."
                );
            return;
        }

        QString tblName = selectedTable->text();

        QMessageBox::StandardButton answer = QMessageBox::question(
            &window,
            "Confirm Delete",
            "Are you sure you want to delete the selected record?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            );

        if (answer != QMessageBox::Yes)
            return;

        QString filePath =
            QDir(projectPath).filePath(
                selectedDB->text() + "/" + tblName + ".csv"
                );

        // READ THE CSV HEADER

        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(
                &window,
                "File Error",
                "Could not open the table file."
                );
            return;
        }

        QTextStream in(&file);

        if (in.atEnd()) {
            file.close();

            QMessageBox::warning(
                &window,
                "Empty Table",
                "The table file is empty."
                );
            return;
        }

        QString headerLine = in.readLine();
        file.close();

        // REWRITE THE FILE WITHOUT THE SELECTED RECORD

        if (!file.open(QIODevice::WriteOnly |
                       QIODevice::Truncate |
                       QIODevice::Text)) {

            QMessageBox::critical(
                &window,
                "Save Error",
                "Could not update the table file."
                );
            return;
        }

        QTextStream out(&file);
        out << headerLine << '\n';

        for (int row = 0; row < table->rowCount(); row++) {

            if (row == selectedRow)
                continue;

            QStringList values;

            for (int col = 0; col < table->columnCount(); col++) {

                QString value;

                if (table->item(row, col))
                    value = table->item(row, col)->text();

                values.append(value);
            }

            out << values.join(',') << '\n';
        }

        file.close();

        // REMOVE THE RECORD FROM THE GUI

        table->removeRow(selectedRow);

        window.statusBar()->showMessage(
            "Record deleted from " + tblName
            );

        QMessageBox::information(
            &window,
            "Success",
            "Record deleted successfully!"
            );
    });


    // SEARCH RECORD

    QObject::connect(search, &QPushButton::clicked, [&]() {

        QListWidgetItem *selectedDB = databaseList->currentItem();
        QListWidgetItem *selectedTable = tableList->currentItem();

        if (!selectedDB || !selectedTable) {
            QMessageBox::warning(
                &window,
                "No Table Selected",
                "Please select a database and table first."
                );
            return;
        }

        if (table->rowCount() == 0) {
            QMessageBox::information(
                &window,
                "No Records",
                "There are no records to search."
                );
            return;
        }

        bool ok;

        QString keyword = QInputDialog::getText(
            &window,
            "Search Records",
            "Enter text to search in all columns:",
            QLineEdit::Normal,
            "",
            &ok
            );

        if (!ok)
            return;

        keyword = keyword.trimmed();

        if (keyword.isEmpty()) {
            QMessageBox::warning(
                &window,
                "Empty Search",
                "Please enter something to search for."
                );
            return;
        }

        // FIND MATCHES IN ALL COLUMNS

        QList<int> matchingRows;

        for (int row = 0; row < table->rowCount(); row++) {

            bool found = false;

            for (int col = 0; col < table->columnCount(); col++) {

                QTableWidgetItem *cell = table->item(row, col);

                if (cell && cell->text().contains(
                        keyword, Qt::CaseInsensitive)) {

                    found = true;
                    break;
                }
            }

            if (found)
                matchingRows.append(row);
        }

        // DISPLAY SEARCH RESULT

        if (matchingRows.isEmpty()) {

            window.statusBar()->showMessage(
                "No matching records found."
                );

            QMessageBox::information(
                &window,
                "Search Result",
                "No matching records found."
                );

            return;
        }

        int firstMatch = matchingRows.first();

        table->selectRow(firstMatch);
        table->setCurrentCell(firstMatch, 0);

        if (table->item(firstMatch, 0))
            table->scrollToItem(table->item(firstMatch, 0));

        window.statusBar()->showMessage(
            QString::number(matchingRows.size()) +
            " matching record(s) found."
            );

        QMessageBox::information(
            &window,
            "Search Result",
            QString::number(matchingRows.size()) +
                " matching record(s) found.\n\n"
                "The first matching record has been selected."
            );
    });

    // SEARCH IN A SELECTED COLUMN

    QObject::connect(searchColumn, &QPushButton::clicked, [&]() {

        if (table->columnCount() == 0 || table->rowCount() == 0) {
            QMessageBox::information(
                &window,
                "No Records",
                "Load a table containing records first."
                );
            return;
        }

        QStringList columns;

        for (int col = 0; col < table->columnCount(); col++)
            columns.append(table->horizontalHeaderItem(col)->text());

        bool ok;

        QString selectedColumn = QInputDialog::getItem(
            &window,
            "Select Column",
            "Choose the column to search:",
            columns,
            0,
            false,
            &ok
            );

        if (!ok)
            return;

        int columnIndex = columns.indexOf(selectedColumn);

        QString keyword = QInputDialog::getText(
                              &window,
                              "Search Column",
                              "Enter search text:",
                              QLineEdit::Normal,
                              "",
                              &ok
                              ).trimmed();

        if (!ok)
            return;

        if (keyword.isEmpty()) {
            QMessageBox::warning(
                &window,
                "Empty Search",
                "Please enter a search value."
                );
            return;
        }

        QList<int> matches;

        for (int row = 0; row < table->rowCount(); row++) {
            QTableWidgetItem *cell = table->item(row, columnIndex);

            if (cell && cell->text().contains(
                    keyword, Qt::CaseInsensitive)) {
                matches.append(row);
            }
        }

        if (matches.isEmpty()) {
            window.statusBar()->showMessage("No matching records found.");
            QMessageBox::information(
                &window,
                "Search Result",
                "No matching records found in " + selectedColumn + "."
                );
            return;
        }

        int firstMatch = matches.first();
        table->selectRow(firstMatch);
        table->setCurrentCell(firstMatch, columnIndex);

        if (table->item(firstMatch, columnIndex))
            table->scrollToItem(table->item(firstMatch, columnIndex));

        window.statusBar()->showMessage(
            QString::number(matches.size()) + " matching record(s) found in " + selectedColumn + "."
            );

        QMessageBox::information(
            &window,
            "Search Result",
            QString::number(matches.size()) +
                " matching record(s) found in column '" + selectedColumn + "'.\n\n"
                                                                           "The first matching record has been selected."
            );
    });

    // SORT RECORDS BY A SELECTED COLUMN

    QObject::connect(sort, &QPushButton::clicked, [&]() {

        if (table->columnCount() == 0 || table->rowCount() == 0) {
            QMessageBox::information(
                &window,
                "No Records",
                "Load a table containing records first."
                );
            return;
        }

        QStringList columns;

        for (int col = 0; col < table->columnCount(); col++)
            columns.append(table->horizontalHeaderItem(col)->text());

        bool ok;

        QString selectedColumn = QInputDialog::getItem(
            &window,
            "Sort Records",
            "Choose the column to sort by:",
            columns,
            0,
            false,
            &ok
            );

        if (!ok)
            return;

        QStringList orders;
        orders << "Ascending (A to Z / smallest first)"
               << "Descending (Z to A / largest first)";

        QString selectedOrder = QInputDialog::getItem(
            &window,
            "Sort Order",
            "Choose sorting order:",
            orders,
            0,
            false,
            &ok
            );

        if (!ok)
            return;

        int columnIndex = columns.indexOf(selectedColumn);

        Qt::SortOrder order = selectedOrder.startsWith("Ascending")
                                  ? Qt::AscendingOrder
                                  : Qt::DescendingOrder;

        table->sortItems(columnIndex, order);

        window.statusBar()->showMessage(
            "Sorted by " + selectedColumn + " (" +
            (order == Qt::AscendingOrder ? "ascending" : "descending") + ")."
            );
    });

    // MAIN WINDOW

    splitter->addWidget(sidebar);
    splitter->addWidget(workspace);

    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);

    window.setCentralWidget(central);

    window.statusBar()->showMessage("MiniDB | Ready");

    window.show();

    return app.exec();
}