#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QSplitter>
#include <QStatusBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QAbstractItemView>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QSaveFile>
#include <QVector>
#include <QVariant>
#include <algorithm>

QStringList parseCSVLine(const QString &line)
{
    QStringList fields;
    QString field;
    bool quoted = false;

    for (int i = 0; i < line.size(); i++) {
        QChar ch = line[i];

        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                i++;
            } else {
                quoted = !quoted;
            }
        } else if (ch == ',' && !quoted) {
            fields.append(field);
            field.clear();
        } else {
            field += ch;
        }
    }

    fields.append(field);
    return fields;
}

QString makeCSVLine(const QStringList &fields)
{
    QStringList encoded;

    for (QString field : fields) {
        field.replace("\"", "\"\"");
        if (field.contains(',') || field.contains('"'))
            field = "\"" + field + "\"";
        encoded.append(field);
    }

    return encoded.join(',');
}

bool readCSV(const QString &path, QStringList &headers,
             QVector<QStringList> &records, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }

    QTextStream in(&file);
    if (in.atEnd()) {
        error = "The CSV file is empty.";
        return false;
    }

    headers = parseCSVLine(in.readLine());
    records.clear();

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty())
            continue;

        QStringList values = parseCSVLine(line);
        while (values.size() < headers.size())
            values.append("");

        if (values.size() > headers.size())
            values = values.mid(0, headers.size());

        records.append(values);
    }

    return true;
}

bool writeCSV(const QString &path, const QStringList &headers,
              const QVector<QStringList> &records, QString &error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }

    QTextStream out(&file);
    out << makeCSVLine(headers) << '\n';

    for (const QStringList &record : records)
        out << makeCSVLine(record) << '\n';

    if (!file.commit()) {
        error = file.errorString();
        return false;
    }

    return true;
}

class NumericTableWidgetItem : public QTableWidgetItem
{
public:
    explicit NumericTableWidgetItem(const QString &value)
        : QTableWidgetItem(value) {}

    bool operator<(const QTableWidgetItem &other) const override
    {
        bool aOk = false, bOk = false;
        double a = text().toDouble(&aOk);
        double b = other.text().toDouble(&bOk);

        if (aOk && bOk)
            return a < b;

        return QString::localeAwareCompare(text(), other.text()) < 0;
    }
};

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
        QLabel { color: #E5E7EB; }
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
            padding: 9px 12px;
        }
        QPushButton:hover { background-color: #345477; }
        QPushButton:pressed { background-color: #1D3854; }
        QStatusBar { background-color: #191F2B; }
        QLineEdit {
            background-color: #191F2B;
            border: 1px solid #394B63;
            border-radius: 5px;
            padding: 6px;
            color: #E5E7EB;
        }
    )");

    QMainWindow window;
    window.setWindowTitle("MiniDB - Database Manager");
    window.resize(1200, 760);

    QString dataPath =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("data");

    if (!QDir().mkpath(dataPath)) {
        QMessageBox::critical(nullptr, "Storage Error",
                              "Cannot create data directory:\n" + dataPath);
        return 1;
    }

    QString currentDatabase;
    QString currentTable;
    QStringList headers;
    QVector<QStringList> records;
    QVector<int> visibleRows;
    bool filterActive = false;
    bool sortActive = false;
    int sortColumn = -1;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;

    auto currentFilePath = [&]() {
        return QDir(dataPath).filePath(
            currentDatabase + "/" + currentTable + ".csv");
    };

    QWidget *central = new QWidget;
    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    QSplitter *splitter = new QSplitter;

    QWidget *sidebar = new QWidget;
    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);

    QLabel *logo = new QLabel("MiniDB");
    logo->setStyleSheet("font-size: 25px; font-weight: bold; padding: 12px;");

    QPushButton *newDB = new QPushButton("+ New Database");
    QListWidget *databaseList = new QListWidget;
    QListWidget *tableList = new QListWidget;

    sideLayout->addWidget(logo);
    sideLayout->addWidget(newDB);
    sideLayout->addWidget(new QLabel("DATABASES"));
    sideLayout->addWidget(databaseList);
    sideLayout->addWidget(new QLabel("TABLES"));
    sideLayout->addWidget(tableList);

    QWidget *workspace = new QWidget;
    QVBoxLayout *workLayout = new QVBoxLayout(workspace);

    QLabel *heading = new QLabel("No Database Selected");
    heading->setStyleSheet("font-size: 23px; font-weight: bold; padding: 10px;");
    QLabel *tableName = new QLabel("No Table Selected");
    QLabel *filterLabel = new QLabel("No active filter");

    QTableWidget *table = new QTableWidget(0, 0);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSortingEnabled(false);

    QPushButton *createTable = new QPushButton("+ Create Table");
    QPushButton *insert = new QPushButton("Insert");
    QPushButton *update = new QPushButton("Update");
    QPushButton *remove = new QPushButton("Delete");
    QPushButton *search = new QPushButton("Search All");
    QPushButton *searchColumn = new QPushButton("Search Column");
    QPushButton *sort = new QPushButton("Sort");
    QPushButton *filter = new QPushButton("Filter");
    QPushButton *clearFilter = new QPushButton("Clear Filter");
    QPushButton *resetSort = new QPushButton("Reset Sort");
    QPushButton *stats = new QPushButton("Statistics");
    QPlainTextEdit *queryInput = new QPlainTextEdit;
    queryInput->setPlaceholderText("Example: SELECT * FROM students WHERE age = 20;");
    queryInput->setMaximumHeight(85);
    QPushButton *executeQuery = new QPushButton("Execute SQL");
    QTableWidget *queryResult = new QTableWidget;
    queryResult->setMinimumHeight(150);
    queryResult->setSelectionBehavior(QAbstractItemView::SelectRows);
    queryResult->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    QLabel *queryStatus = new QLabel("SQL Console | Read-only SELECT queries");

    QHBoxLayout *actions1 = new QHBoxLayout;
    actions1->addWidget(createTable);
    actions1->addWidget(insert);
    actions1->addWidget(update);
    actions1->addWidget(remove);
    actions1->addWidget(search);
    actions1->addWidget(searchColumn);

    QHBoxLayout *actions2 = new QHBoxLayout;
    actions2->addWidget(sort);
    actions2->addWidget(filter);
    actions2->addWidget(clearFilter);
    actions2->addWidget(resetSort);
    actions2->addWidget(stats);

    workLayout->addWidget(heading);
    workLayout->addWidget(tableName);
    workLayout->addWidget(filterLabel);
    workLayout->addWidget(table);
    workLayout->addLayout(actions1);
    workLayout->addLayout(actions2);
    workLayout->addWidget(new QLabel("SQL QUERY CONSOLE"));
    workLayout->addWidget(queryInput);
    workLayout->addWidget(executeQuery);
    workLayout->addWidget(queryStatus);
    workLayout->addWidget(queryResult);

    auto refreshView = [&]() {
        visibleRows.clear();

        for (int i = 0; i < records.size(); i++) {
            if (!filterActive) {
                visibleRows.append(i);
                continue;
            }

            int col = -1;
            for (int c = 0; c < headers.size(); c++) {
                if (headers[c] == filterLabel->property("filterColumn").toString()) {
                    col = c;
                    break;
                }
            }

            if (col >= 0 && col < records[i].size() &&
                records[i][col].contains(
                    filterLabel->property("filterValue").toString(),
                    Qt::CaseInsensitive)) {
                visibleRows.append(i);
            }
        }

        if (sortActive && sortColumn >= 0 && sortColumn < headers.size()) {
            std::stable_sort(visibleRows.begin(), visibleRows.end(),
                             [&](int left, int right) {
                                 QString a = records[left].value(sortColumn);
                                 QString b = records[right].value(sortColumn);

                                 bool aOk = false, bOk = false;
                                 double av = a.toDouble(&aOk);
                                 double bv = b.toDouble(&bOk);

                                 bool less;
                                 if (aOk && bOk)
                                     less = av < bv;
                                 else
                                     less = QString::localeAwareCompare(a, b) < 0;

                                 return sortOrder == Qt::AscendingOrder ? less : !less && a != b;
                             });
        }

        table->setSortingEnabled(false);
        table->clear();
        table->setColumnCount(headers.size());
        table->setRowCount(visibleRows.size());
        table->setHorizontalHeaderLabels(headers);

        for (int row = 0; row < visibleRows.size(); row++) {
            const QStringList &record = records[visibleRows[row]];
            for (int col = 0; col < headers.size(); col++) {
                table->setItem(row, col,
                               new NumericTableWidgetItem(record.value(col)));
            }
        }

        if (filterActive) {
            filterLabel->setText(
                "Filter: " + filterLabel->property("filterColumn").toString() +
                " contains \"" + filterLabel->property("filterValue").toString() +
                "\" | Showing " + QString::number(visibleRows.size()) +
                " of " + QString::number(records.size()));
        } else {
            filterLabel->setText("No active filter | " +
                                 QString::number(records.size()) + " record(s)");
        }
    };

    auto loadSelectedTable = [&]() {
        if (currentDatabase.isEmpty() || currentTable.isEmpty())
            return;

        QString error;
        if (!readCSV(currentFilePath(), headers, records, error)) {
            QMessageBox::critical(&window, "File Error",
                                  "Could not load table:\n" + error);
            return;
        }

        filterActive = false;
        sortActive = false;
        sortColumn = -1;
        filterLabel->setProperty("filterColumn", "");
        filterLabel->setProperty("filterValue", "");
        tableName->setText("Table: " + currentTable);
        refreshView();
        window.statusBar()->showMessage("Loaded table: " + currentTable);
    };

    QDir root(dataPath);
    const QStringList existingDBs =
        root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &db : existingDBs)
        databaseList->addItem(db);

    QObject::connect(databaseList, &QListWidget::currentTextChanged,
                     [&](const QString &name) {
                         currentDatabase = name;
                         currentTable.clear();
                         tableList->clear();
                         headers.clear();
                         records.clear();
                         table->clear();
                         table->setRowCount(0);
                         table->setColumnCount(0);
                         tableName->setText("No Table Selected");
                         filterLabel->setText("No active filter");

                         if (name.isEmpty())
                             return;

                         heading->setText(name);
                         QDir dbFolder(QDir(dataPath).filePath(name));
                         const QStringList files =
                             dbFolder.entryList(QStringList() << "*.csv", QDir::Files);

                         for (const QString &file : files) {
                             QString tableNameOnly = file;
                             tableNameOnly.chop(4);
                             tableList->addItem(tableNameOnly);
                         }

                         window.statusBar()->showMessage("Selected database: " + name);
                     });

    QObject::connect(tableList, &QListWidget::currentTextChanged,
                     [&](const QString &name) {
                         currentTable = name;
                         if (name.isEmpty())
                             return;
                         loadSelectedTable();
                     });

    QObject::connect(newDB, &QPushButton::clicked, [&]() {
        bool ok = false;
        QString name = QInputDialog::getText(
                           &window, "Create Database", "Enter database name:",
                           QLineEdit::Normal, "", &ok).trimmed();

        if (!ok)
            return;

        QRegularExpression validName("^[A-Za-z0-9_-]+$");
        if (name.isEmpty() || !validName.match(name).hasMatch()) {
            QMessageBox::warning(&window, "Invalid Name",
                                 "Use only letters, numbers, underscores and hyphens.");
            return;
        }

        for (int i = 0; i < databaseList->count(); i++) {
            if (databaseList->item(i)->text().compare(
                    name, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(&window, "Error",
                                     "Database already exists.");
                return;
            }
        }

        if (!QDir(dataPath).mkdir(name)) {
            QMessageBox::critical(&window, "Creation Failed",
                                  "Could not create database folder.");
            return;
        }

        databaseList->addItem(name);
        databaseList->setCurrentRow(databaseList->count() - 1);
        window.statusBar()->showMessage("Database created: " + name);
    });

    QObject::connect(createTable, &QPushButton::clicked, [&]() {
        if (currentDatabase.isEmpty()) {
            QMessageBox::warning(&window, "No Database",
                                 "Please select a database first.");
            return;
        }

        bool ok = false;
        QString name = QInputDialog::getText(
                           &window, "Create Table", "Enter table name:",
                           QLineEdit::Normal, "", &ok).trimmed();
        if (!ok)
            return;

        QRegularExpression validName("^[A-Za-z0-9_-]+$");
        if (name.isEmpty() || !validName.match(name).hasMatch()) {
            QMessageBox::warning(&window, "Invalid Name",
                                 "Use only letters, numbers, underscores and hyphens.");
            return;
        }

        QStringList columns = QInputDialog::getText(
                                  &window, "Table Columns",
                                  "Enter column names separated by commas (e.g. id,name,age):",
                                  QLineEdit::Normal, "", &ok).split(',');
        if (!ok)
            return;

        QSet<QString> unique;
        for (QString &column : columns) {
            column = column.trimmed();
            if (column.isEmpty() || !validName.match(column).hasMatch() ||
                unique.contains(column.toLower())) {
                QMessageBox::warning(&window, "Invalid Columns",
                                     "Column names must be valid and unique.");
                return;
            }
            unique.insert(column.toLower());
        }

        QString path = QDir(dataPath).filePath(
            currentDatabase + "/" + name + ".csv");
        if (QFile::exists(path)) {
            QMessageBox::warning(&window, "Error", "Table already exists.");
            return;
        }

        QString error;
        if (!writeCSV(path, columns, {}, error)) {
            QMessageBox::critical(&window, "Save Error", error);
            return;
        }

        tableList->addItem(name);
        tableList->setCurrentRow(tableList->count() - 1);
        window.statusBar()->showMessage("Table created: " + name);
    });

    QObject::connect(insert, &QPushButton::clicked, [&]() {
        if (currentTable.isEmpty()) {
            QMessageBox::warning(&window, "No Table",
                                 "Please select a table first.");
            return;
        }

        QStringList values;
        for (const QString &column : headers) {
            bool ok = false;
            QString value = QInputDialog::getText(
                &window, "Insert Record", "Enter value for " + column + ":",
                QLineEdit::Normal, "", &ok);
            if (!ok)
                return;
            if (value.contains('\n') || value.contains('\r')) {
                QMessageBox::warning(&window, "Invalid Value",
                                     "Line breaks are not supported.");
                return;
            }
            values.append(value);
        }

        records.append(values);
        QString error;
        if (!writeCSV(currentFilePath(), headers, records, error)) {
            records.removeLast();
            QMessageBox::critical(&window, "Save Error", error);
            return;
        }

        refreshView();
        window.statusBar()->showMessage("Record inserted.");
    });

    QObject::connect(update, &QPushButton::clicked, [&]() {
        int viewRow = table->currentRow();
        if (currentTable.isEmpty() || viewRow < 0 || viewRow >= visibleRows.size()) {
            QMessageBox::warning(&window, "No Record",
                                 "Select a record to update.");
            return;
        }

        int dataRow = visibleRows[viewRow];
        QStringList updated = records[dataRow];

        for (int col = 0; col < headers.size(); col++) {
            bool ok = false;
            QString value = QInputDialog::getText(
                &window, "Update Record",
                "Enter new value for " + headers[col] + ":",
                QLineEdit::Normal, updated.value(col), &ok);
            if (!ok)
                return;
            if (value.contains('\n') || value.contains('\r')) {
                QMessageBox::warning(&window, "Invalid Value",
                                     "Line breaks are not supported.");
                return;
            }
            updated[col] = value;
        }

        QStringList old = records[dataRow];
        records[dataRow] = updated;
        QString error;
        if (!writeCSV(currentFilePath(), headers, records, error)) {
            records[dataRow] = old;
            QMessageBox::critical(&window, "Save Error", error);
            return;
        }

        refreshView();
        window.statusBar()->showMessage("Record updated.");
    });

    QObject::connect(remove, &QPushButton::clicked, [&]() {
        int viewRow = table->currentRow();
        if (currentTable.isEmpty() || viewRow < 0 || viewRow >= visibleRows.size()) {
            QMessageBox::warning(&window, "No Record",
                                 "Select a record to delete.");
            return;
        }

        if (QMessageBox::question(&window, "Confirm Delete",
                                  "Delete the selected record?",
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes)
            return;

        int dataRow = visibleRows[viewRow];
        QStringList removed = records.takeAt(dataRow);
        QString error;
        if (!writeCSV(currentFilePath(), headers, records, error)) {
            records.insert(dataRow, removed);
            QMessageBox::critical(&window, "Save Error", error);
            return;
        }

        refreshView();
        window.statusBar()->showMessage("Record deleted.");
    });

    QObject::connect(search, &QPushButton::clicked, [&]() {
        if (records.isEmpty()) {
            QMessageBox::information(&window, "No Records",
                                     "There are no records to search.");
            return;
        }

        bool ok = false;
        QString keyword = QInputDialog::getText(
                              &window, "Search All", "Enter text to search in all columns:",
                              QLineEdit::Normal, "", &ok).trimmed();
        if (!ok || keyword.isEmpty())
            return;

        int firstMatch = -1;
        int count = 0;
        for (int row = 0; row < visibleRows.size(); row++) {
            const QStringList &record = records[visibleRows[row]];
            bool found = false;
            for (const QString &value : record) {
                if (value.contains(keyword, Qt::CaseInsensitive)) {
                    found = true;
                    break;
                }
            }
            if (found) {
                if (firstMatch < 0)
                    firstMatch = row;
                count++;
            }
        }

        if (firstMatch >= 0) {
            table->selectRow(firstMatch);
            table->scrollToItem(table->item(firstMatch, 0));
        }

        QMessageBox::information(&window, "Search Result",
                                 count == 0 ? "No matching records found."
                                            : QString::number(count) +
                                                  " matching record(s). First match selected.");
    });

    QObject::connect(searchColumn, &QPushButton::clicked, [&]() {
        if (records.isEmpty() || headers.isEmpty()) {
            QMessageBox::information(&window, "No Records",
                                     "Load a table containing records first.");
            return;
        }

        bool ok = false;
        QString column = QInputDialog::getItem(
            &window, "Search Column", "Choose a column:", headers, 0, false, &ok);
        if (!ok)
            return;

        QString keyword = QInputDialog::getText(
                              &window, "Search Column", "Enter search text:",
                              QLineEdit::Normal, "", &ok).trimmed();
        if (!ok || keyword.isEmpty())
            return;

        int col = headers.indexOf(column);
        int firstMatch = -1, count = 0;
        for (int row = 0; row < visibleRows.size(); row++) {
            if (records[visibleRows[row]].value(col).contains(
                    keyword, Qt::CaseInsensitive)) {
                if (firstMatch < 0)
                    firstMatch = row;
                count++;
            }
        }

        if (firstMatch >= 0) {
            table->selectRow(firstMatch);
            table->setCurrentCell(firstMatch, col);
            table->scrollToItem(table->item(firstMatch, col));
        }

        QMessageBox::information(&window, "Search Result",
                                 count == 0 ? "No matching records found."
                                            : QString::number(count) + " matching record(s) in " +
                                                  column + ". First match selected.");
    });

    QObject::connect(sort, &QPushButton::clicked, [&]() {
        if (records.isEmpty() || headers.isEmpty()) {
            QMessageBox::information(&window, "No Records",
                                     "Load a table containing records first.");
            return;
        }

        bool ok = false;
        QString column = QInputDialog::getItem(
            &window, "Sort Records", "Choose a column:", headers, 0, false, &ok);
        if (!ok)
            return;

        QStringList orders = {
            "Ascending (smallest first / A to Z)",
            "Descending (largest first / Z to A)"
        };
        QString order = QInputDialog::getItem(
            &window, "Sort Order", "Choose sorting order:",
            orders, 0, false, &ok);
        if (!ok)
            return;

        sortColumn = headers.indexOf(column);
        sortOrder = order.startsWith("Ascending")
                        ? Qt::AscendingOrder : Qt::DescendingOrder;
        sortActive = true;
        refreshView();
        window.statusBar()->showMessage("Sorted by " + column + ".");
    });

    QObject::connect(filter, &QPushButton::clicked, [&]() {
        if (records.isEmpty() || headers.isEmpty()) {
            QMessageBox::information(&window, "No Records",
                                     "Load a table containing records first.");
            return;
        }

        bool ok = false;
        QString column = QInputDialog::getItem(
            &window, "Filter Records", "Choose a column:", headers, 0, false, &ok);
        if (!ok)
            return;

        QString value = QInputDialog::getText(
                            &window, "Filter Records",
                            "Show records where " + column + " contains:",
                            QLineEdit::Normal, "", &ok).trimmed();
        if (!ok || value.isEmpty())
            return;

        filterLabel->setProperty("filterColumn", column);
        filterLabel->setProperty("filterValue", value);
        filterActive = true;
        refreshView();

        window.statusBar()->showMessage(
            QString::number(visibleRows.size()) + " record(s) match the filter.");
    });

    QObject::connect(clearFilter, &QPushButton::clicked, [&]() {
        filterActive = false;
        filterLabel->setProperty("filterColumn", "");
        filterLabel->setProperty("filterValue", "");
        refreshView();
        window.statusBar()->showMessage("Filter cleared.");
    });

    QObject::connect(resetSort, &QPushButton::clicked, [&]() {
        sortActive = false;
        sortColumn = -1;
        refreshView();
        window.statusBar()->showMessage("Original record order restored.");
    });

    QObject::connect(stats, &QPushButton::clicked, [&]() {
        if (currentDatabase.isEmpty()) {
            QMessageBox::information(&window, "Statistics",
                                     "Select a database first.");
            return;
        }

        QDir dbDir(QDir(dataPath).filePath(currentDatabase));
        QStringList csvFiles =
            dbDir.entryList(QStringList() << "*.csv", QDir::Files);

        int totalRecords = 0;
        QString tableDetails;

        for (const QString &fileName : csvFiles) {
            QString path = dbDir.filePath(fileName);
            QStringList cols;
            QVector<QStringList> rows;
            QString error;

            if (readCSV(path, cols, rows, error)) {
                totalRecords += rows.size();
                QString name = fileName;
                name.chop(4);
                tableDetails += name + ": " +
                                QString::number(rows.size()) + " records, " +
                                QString::number(cols.size()) + " columns\n";
            }
        }

        QString selectedDetails;
        if (!currentTable.isEmpty()) {
            selectedDetails = "\nSelected table: " + currentTable +
                              "\nRows: " + QString::number(records.size()) +
                              "\nColumns: " + QString::number(headers.size());
        }

        QMessageBox::information(&window, "Database Statistics",
                                 "Database: " + currentDatabase +
                                     "\nTotal tables: " + QString::number(csvFiles.size()) +
                                     "\nTotal records: " + QString::number(totalRecords) +
                                     "\n\nTable details:\n" + tableDetails + selectedDetails);
    });


    QObject::connect(executeQuery, &QPushButton::clicked, [&]() {
        QString sql = queryInput->toPlainText().trimmed();

        if (currentDatabase.isEmpty()) {
            QMessageBox::warning(&window, "No Database",
                                 "Select a database before running a query.");
            return;
        }

        if (sql.endsWith(';'))
            sql.chop(1);
        sql = sql.trimmed();

        QRegularExpression selectPattern(
            "^SELECT\\s+(.+?)\\s+FROM\\s+([A-Za-z0-9_-]+)"
            "(?:\\s+WHERE\\s+([A-Za-z0-9_-]+)\\s*=\\s*"
            "(?:'([^']*)'|\"([^\"]*)\"|([^\\s;]+)))?\\s*$",
            QRegularExpression::CaseInsensitiveOption);

        QRegularExpressionMatch match = selectPattern.match(sql);

        if (!match.hasMatch()) {
            queryStatus->setText(
                "Invalid query. Supported format: SELECT columns FROM table "
                "[WHERE column = value]");
            QMessageBox::warning(&window, "SQL Syntax Error",
                                 "This console currently supports SELECT queries only.\n\n"
                                 "Examples:\n"
                                 "SELECT * FROM students\n"
                                 "SELECT name, age FROM students\n"
                                 "SELECT * FROM students WHERE age = 20");
            return;
        }

        QString selectedColumnsText = match.captured(1).trimmed();
        QString targetTable = match.captured(2).trimmed();
        QString whereColumn = match.captured(3).trimmed();
        QString whereValue = match.captured(4);
        if (whereValue.isNull())
            whereValue = match.captured(5);
        if (whereValue.isNull())
            whereValue = match.captured(6);

        QString targetPath = QDir(dataPath).filePath(
            currentDatabase + "/" + targetTable + ".csv");

        if (!QFile::exists(targetPath)) {
            queryStatus->setText("Table not found: " + targetTable);
            QMessageBox::warning(&window, "Table Not Found",
                                 "Table '" + targetTable +
                                     "' does not exist in the selected database.");
            return;
        }

        QStringList queryHeaders;
        QVector<QStringList> queryRecords;
        QString error;

        if (!readCSV(targetPath, queryHeaders, queryRecords, error)) {
            QMessageBox::critical(&window, "Query Error",
                                  "Could not read table:\n" + error);
            return;
        }

        int whereIndex = -1;
        if (!whereColumn.isEmpty()) {
            for (int i = 0; i < queryHeaders.size(); i++) {
                if (queryHeaders[i].compare(
                        whereColumn, Qt::CaseInsensitive) == 0) {
                    whereIndex = i;
                    break;
                }
            }

            if (whereIndex < 0) {
                queryStatus->setText("Unknown WHERE column: " + whereColumn);
                QMessageBox::warning(&window, "Column Not Found",
                                     "Column '" + whereColumn +
                                         "' does not exist.");
                return;
            }
        }

        QStringList outputHeaders;
        if (selectedColumnsText == "*") {
            outputHeaders = queryHeaders;
        } else {
            QStringList requestedColumns =
                selectedColumnsText.split(',', Qt::SkipEmptyParts);

            for (QString column : requestedColumns) {
                column = column.trimmed();
                int index = -1;
                for (int i = 0; i < queryHeaders.size(); i++) {
                    if (queryHeaders[i].compare(
                            column, Qt::CaseInsensitive) == 0) {
                        index = i;
                        break;
                    }
                }

                if (index < 0) {
                    QMessageBox::warning(&window, "Column Not Found",
                                         "Column '" + column +
                                             "' does not exist.");
                    return;
                }

                if (outputHeaders.contains(queryHeaders[index])) {
                    QMessageBox::warning(&window, "SQL Error",
                                         "A column was selected more than once.");
                    return;
                }
                outputHeaders.append(queryHeaders[index]);
            }

            if (outputHeaders.isEmpty()) {
                QMessageBox::warning(&window, "SQL Error",
                                     "Select at least one column.");
                return;
            }
        }

        QVector<int> outputIndexes;
        for (const QString &column : outputHeaders)
            outputIndexes.append(queryHeaders.indexOf(column));

        QVector<QStringList> outputRecords;
        for (const QStringList &record : queryRecords) {
            if (whereIndex >= 0 &&
                record.value(whereIndex).compare(
                    whereValue, Qt::CaseInsensitive) != 0)
                continue;

            QStringList outputRecord;
            for (int index : outputIndexes)
                outputRecord.append(record.value(index));
            outputRecords.append(outputRecord);
        }

        queryResult->clear();
        queryResult->setColumnCount(outputHeaders.size());
        queryResult->setRowCount(outputRecords.size());
        queryResult->setHorizontalHeaderLabels(outputHeaders);

        for (int row = 0; row < outputRecords.size(); row++) {
            for (int col = 0; col < outputHeaders.size(); col++) {
                queryResult->setItem(
                    row, col,
                    new QTableWidgetItem(outputRecords[row].value(col)));
            }
        }

        queryStatus->setText(
            "Query successful | Table: " + targetTable +
            " | " + QString::number(outputRecords.size()) +
            " row(s) returned");
        window.statusBar()->showMessage(
            "SQL SELECT completed: " +
            QString::number(outputRecords.size()) + " row(s).");
    });

    if (databaseList->count() > 0)
        databaseList->setCurrentRow(0);

    splitter->addWidget(sidebar);
    splitter->addWidget(workspace);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter);

    window.setCentralWidget(central);
    window.statusBar()->showMessage("MiniDB | Ready");
    window.show();

    return app.exec();
}
