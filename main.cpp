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

    table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch
        );

    // BUTTONS

    QPushButton *createTable = new QPushButton("+ Create Table");
    QPushButton *insert = new QPushButton("Insert");
    QPushButton *update = new QPushButton("Update");
    QPushButton *remove = new QPushButton("Delete");
    QPushButton *search = new QPushButton("Search");

    QHBoxLayout *actions = new QHBoxLayout;

    actions->addWidget(createTable);
    actions->addWidget(insert);
    actions->addWidget(update);
    actions->addWidget(remove);
    actions->addWidget(search);

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