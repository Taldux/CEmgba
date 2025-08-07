#include "CheatEngine.h"
#include "CoreController.h"
#include <QtWidgets/QApplication>
#include <QtCore/QTime>
#include <QtCore/QRandomGenerator>
#include <qstatusbar.h>

using QGBA::CoreController; 


CheatEngine::CheatEngine(std::shared_ptr<CoreController> controller, QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_refreshTimer(new QTimer(this))
    , m_controller(std::move(controller))
{
    setupUI();

    // Setup auto refresh timer
    m_refreshTimer->setInterval(1000); // 1 second
    connect(m_refreshTimer, &QTimer::timeout, this, &CheatEngine::onMemoryRefresh);
    m_refreshTimer->start();

    // Initialize with some sample data
    updateMemoryTable();
    updateCheatTable();
}
CheatEngine::~CheatEngine()
{
}

void CheatEngine::setupUI()
{
    setWindowTitle("GBA Cheat Engine");
    setMinimumSize(1000, 700);

    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);

    // Main splitter 
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // Right splitter 
    m_rightSplitter = new QSplitter(Qt::Vertical);

    setupMemoryViewer();
    setupSearchPanel();
    setupCheatManager();

    // Add panels to splitters
    m_mainSplitter->addWidget(m_memoryGroup);
    m_mainSplitter->addWidget(m_rightSplitter);

    m_rightSplitter->addWidget(m_searchGroup);
    m_rightSplitter->addWidget(m_cheatGroup);
    m_rightSplitter->addWidget(m_editorGroup);

    // Setup log output
    m_logOutput = new QTextEdit;
    m_logOutput->setMaximumHeight(100);
    m_logOutput->setReadOnly(true);
    m_logOutput->append("Cheat Engine started...");

    // Status label
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel);

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_mainSplitter);
    mainLayout->addWidget(m_logOutput);

    m_centralWidget->setLayout(mainLayout);

    // Set splitter proportions
    m_mainSplitter->setSizes({500, 500});
    m_rightSplitter->setSizes({200, 300, 150});
}

void CheatEngine::setupMemoryViewer()
{
    m_memoryGroup = new QGroupBox("Memory Viewer");

    // Memory table
    m_memoryTable = new QTableWidget(0, 4);
    QStringList headers = {"adress", "value", "old value", "type"};
    m_memoryTable->setHorizontalHeaderLabels(headers);
    m_memoryTable->horizontalHeader()->setStretchLastSection(true);
    m_memoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_memoryTable->setAlternatingRowColors(true);

    // Controls
    QHBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->addWidget(new QLabel("adress:"));

    m_addressInput = new QLineEdit;
    m_addressInput->setPlaceholderText("0x8000");
    controlLayout->addWidget(m_addressInput);

    m_gotoButton = new QPushButton("Go to");
    controlLayout->addWidget(m_gotoButton);

    m_refreshButton = new QPushButton("Refresh");
    controlLayout->addWidget(m_refreshButton);

    controlLayout->addStretch();

    // Layout
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(controlLayout);
    layout->addWidget(m_memoryTable);

    m_memoryGroup->setLayout(layout);

    // Connect signals
    connect(m_refreshButton, &QPushButton::clicked, this, &CheatEngine::onMemoryRefresh);
    connect(m_memoryTable, &QTableWidget::itemSelectionChanged, this, &CheatEngine::onMemoryTableSelectionChanged);
}

void CheatEngine::setupSearchPanel()
{
    m_searchGroup = new QGroupBox("Memory Search");

    QGridLayout *layout = new QGridLayout;

    //value
    layout->addWidget(new QLabel("Value:"), 0, 0);
    m_searchValue = new QLineEdit;
    m_searchValue->setPlaceholderText("255");
    layout->addWidget(m_searchValue, 0, 1);

    //type
    layout->addWidget(new QLabel("Type:"), 1, 0);
    m_searchType = new QComboBox;
    m_searchType->addItems({"Byte (8-bit)", "Word (16-bit)", "Text"});
    layout->addWidget(m_searchType, 1, 1);

    // Compare type
    layout->addWidget(new QLabel("Compare:"), 2, 0);
    m_compareType = new QComboBox;
    m_compareType->addItems({"Equal", "Greater", "Less", "Changed", "Unchanged"});
    layout->addWidget(m_compareType, 2, 1);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    m_searchButton = new QPushButton("First Scan");
    m_nextScanButton = new QPushButton("Next Scan");
    m_resetButton = new QPushButton("Reset");

    buttonLayout->addWidget(m_searchButton);
    buttonLayout->addWidget(m_nextScanButton);
    buttonLayout->addWidget(m_resetButton);

    layout->addLayout(buttonLayout, 3, 0, 1, 2);

    // Results label
    m_resultsLabel = new QLabel("Results: 0");
    layout->addWidget(m_resultsLabel, 4, 0, 1, 2);

    m_searchGroup->setLayout(layout);

    // Connect signals
    connect(m_searchButton, &QPushButton::clicked, this, &CheatEngine::onSearchMemory);
    connect(m_nextScanButton, &QPushButton::clicked, this, &CheatEngine::onSearchMemory);
    connect(m_resetButton, &QPushButton::clicked, [this]() {
        m_searchResults.clear();
        m_resultsLabel->setText("Results: 0");
        m_logOutput->append("Scan reset");
    });
}

void CheatEngine::setupCheatManager()
{
    m_cheatGroup = new QGroupBox("Cheat Manager");

    // Cheat table
    m_cheatTable = new QTableWidget(0, 5);
    QStringList headers = {"Actice", "Adress", "Value", "Description", "Freezed"};
    m_cheatTable->setHorizontalHeaderLabels(headers);
    m_cheatTable->horizontalHeader()->setStretchLastSection(true);
    m_cheatTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cheatTable->setAlternatingRowColors(true);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    m_addCheatButton = new QPushButton("Add");
    m_removeCheatButton = new QPushButton("Delete");
    m_toggleFreezeButton = new QPushButton("Freeze Toggle");

    buttonLayout->addWidget(m_addCheatButton);
    buttonLayout->addWidget(m_removeCheatButton);
    buttonLayout->addWidget(m_toggleFreezeButton);
    buttonLayout->addStretch();

    // Cheat editor group
    m_editorGroup = new QGroupBox("Cheat Editor");
    QGridLayout *editorLayout = new QGridLayout;

    editorLayout->addWidget(new QLabel("adress:"), 0, 0);
    m_cheatAddress = new QLineEdit;
    m_cheatAddress->setPlaceholderText("0x8000");
    editorLayout->addWidget(m_cheatAddress, 0, 1);

    editorLayout->addWidget(new QLabel("Value:"), 1, 0);
    m_cheatValue = new QLineEdit;
    m_cheatValue->setPlaceholderText("255");
    editorLayout->addWidget(m_cheatValue, 1, 1);

    editorLayout->addWidget(new QLabel("Description:"), 2, 0);
    m_cheatDescription = new QLineEdit;
    m_cheatDescription->setPlaceholderText("Unendlich Leben");
    editorLayout->addWidget(m_cheatDescription, 2, 1);

    editorLayout->addWidget(new QLabel("Type:"), 3, 0);
    m_cheatType = new QComboBox;
    m_cheatType->addItems({"Byte", "Word"});
    editorLayout->addWidget(m_cheatType, 3, 1);

    m_freezeCheckbox = new QCheckBox("Freeze value");
    editorLayout->addWidget(m_freezeCheckbox, 4, 0, 1, 2);

    m_editorGroup->setLayout(editorLayout);

    // Layout
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(m_cheatTable);
    layout->addLayout(buttonLayout);

    m_cheatGroup->setLayout(layout);

    // Connect signals
    connect(m_addCheatButton, &QPushButton::clicked, this, &CheatEngine::onAddCheat);
    connect(m_removeCheatButton, &QPushButton::clicked, this, &CheatEngine::onRemoveCheat);
    connect(m_toggleFreezeButton, &QPushButton::clicked, this, &CheatEngine::onToggleFreeze);
    connect(m_cheatTable, &QTableWidget::itemSelectionChanged, this, &CheatEngine::onCheatTableSelectionChanged);
}

void CheatEngine::onSearchMemory()
{
    QString value = m_searchValue->text();
    QString type = m_searchType->currentText();
    QString compare = m_compareType->currentText();

    if (value.isEmpty()) {
        m_logOutput->append("Error: Entered no search value");
        return;
    }

    // Simulate memory search
    m_searchResults.clear();

    // some dummy results
    for (int i = 0; i < 10; i++) {
        m_searchResults.append(QString("0x%1").arg(0x8000 + i * 4, 4, 16, QChar('0')));
    }

    m_resultsLabel->setText(QString("Results: %1").arg(m_searchResults.count()));
    m_logOutput->append(QString("Search for '%1' (%2, %3): %4 results").arg(value, type, compare).arg(m_searchResults.count()));
}

void CheatEngine::onAddCheat()
{
    QString address = m_cheatAddress->text();
    QString value = m_cheatValue->text();
    QString description = m_cheatDescription->text();
    QString type = m_cheatType->currentText();
    bool frozen = m_freezeCheckbox->isChecked();

    if (address.isEmpty() || value.isEmpty()) {
        m_logOutput->append("Error: Address and value must be filled in");
        return;
    }

    CheatEntry cheat;
    cheat.address = address;
    cheat.value = value;
    cheat.description = description.isEmpty() ? "Unknown" : description;
    cheat.type = type;
    cheat.frozen = frozen;
    cheat.enabled = true;

    m_cheats.append(cheat);
    updateCheatTable();

    m_logOutput->append(QString("Cheat added: %1 = %2").arg(address, value));

    // Clear editor
    m_cheatAddress->clear();
    m_cheatValue->clear();
    m_cheatDescription->clear();
    m_freezeCheckbox->setChecked(false);
}

void CheatEngine::onRemoveCheat()
{
    int row = m_cheatTable->currentRow();
    if (row >= 0 && row < m_cheats.count()) {
        CheatEntry cheat = m_cheats.at(row);
        m_cheats.removeAt(row);
        updateCheatTable();
        m_logOutput->append(QString("Cheat deleted: %1").arg(cheat.description));
    }
}

void CheatEngine::onToggleFreeze()
{
    int row = m_cheatTable->currentRow();
    if (row >= 0 && row < m_cheats.count()) {
        m_cheats[row].frozen = !m_cheats[row].frozen;
        updateCheatTable();
        m_logOutput->append(QString("Freeze-Status changed: %1").arg(m_cheats[row].description));
    }
}

void CheatEngine::onMemoryRefresh()
{
    updateMemoryTable();
    m_statusLabel->setText(QString("Last refresh: %1").arg(QTime::currentTime().toString()));
}

void CheatEngine::onValueChanged()
{
    // Handle value changes in tables
}

void CheatEngine::onCheatTableSelectionChanged()
{
    int row = m_cheatTable->currentRow();
    if (row >= 0 && row < m_cheats.count()) {
        CheatEntry cheat = m_cheats.at(row);
        m_cheatAddress->setText(cheat.address);
        m_cheatValue->setText(cheat.value);
        m_cheatDescription->setText(cheat.description);
        m_cheatType->setCurrentText(cheat.type);
        m_freezeCheckbox->setChecked(cheat.frozen);
    }
}

void CheatEngine::onMemoryTableSelectionChanged()
{
    int row = m_memoryTable->currentRow();
    if (row >= 0 && row < m_memoryEntries.count()) {
        MemoryEntry entry = m_memoryEntries.at(row);
        m_cheatAddress->setText(entry.address);
        m_cheatValue->setText(entry.value);
    }
}

void CheatEngine::updateMemoryTable()
{
    // Simulate memory data
    m_memoryEntries.clear();

    for (int i = 0; i < 50; i++) {
        MemoryEntry entry;
        entry.address = QString("0x%1").arg(0x8000 + i, 4, 16, QChar('0'));
        entry.value = QString("0x%1").arg(QRandomGenerator::global()->bounded(256), 2, 16, QChar('0'));
        entry.oldValue = QString("0x%1").arg(QRandomGenerator::global()->bounded(256), 2, 16, QChar('0'));
        entry.changed = (QRandomGenerator::global()->bounded(10)) == 0;
        m_memoryEntries.append(entry);
    }

    m_memoryTable->setRowCount(m_memoryEntries.count());

    for (int i = 0; i < m_memoryEntries.count(); i++) {
        MemoryEntry entry = m_memoryEntries.at(i);

        m_memoryTable->setItem(i, 0, new QTableWidgetItem(entry.address));
        m_memoryTable->setItem(i, 1, new QTableWidgetItem(entry.value));
        m_memoryTable->setItem(i, 2, new QTableWidgetItem(entry.oldValue));
        m_memoryTable->setItem(i, 3, new QTableWidgetItem("Byte"));

        if (entry.changed) {
            for (int j = 0; j < 4; j++) {
                m_memoryTable->item(i, j)->setBackground(QColor(255, 255, 200));
            }
        }
    }
}

void CheatEngine::updateCheatTable()
{
    m_cheatTable->setRowCount(m_cheats.count());

    for (int i = 0; i < m_cheats.count(); i++) {
        CheatEntry cheat = m_cheats.at(i);

        QCheckBox *enabledBox = new QCheckBox;
        enabledBox->setChecked(cheat.enabled);
        m_cheatTable->setCellWidget(i, 0, enabledBox);

        m_cheatTable->setItem(i, 1, new QTableWidgetItem(cheat.address));
        m_cheatTable->setItem(i, 2, new QTableWidgetItem(cheat.value));
        m_cheatTable->setItem(i, 3, new QTableWidgetItem(cheat.description));

        QCheckBox *frozenBox = new QCheckBox;
        frozenBox->setChecked(cheat.frozen);
        m_cheatTable->setCellWidget(i, 4, frozenBox);

        if (cheat.frozen) {
            for (int j = 1; j < 4; j++) {
                m_cheatTable->item(i, j)->setBackground(QColor(200, 200, 255));
            }
        }
    }
}
