#ifndef CHEATENGINE_H
#define CHEATENGINE_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QHeaderView>
#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include "CoreController.h" 



class CheatEngine : public QMainWindow
{
    Q_OBJECT

public:
    CheatEngine(std::shared_ptr<QGBA::CoreController> controller, QWidget* parent = nullptr);
    ~CheatEngine();

private slots:
    void onSearchMemory();
    void onAddCheat();
    void onRemoveCheat();
    void onToggleFreeze();
    void onMemoryRefresh();
    void onValueChanged();
    void onCheatTableSelectionChanged();
    void onMemoryTableSelectionChanged();

private:
    void setupUI();
    void setupMemoryViewer();
    void setupCheatManager();
    void setupSearchPanel();
    void updateMemoryTable();
    void updateCheatTable();

    // Main layout components
    QWidget *m_centralWidget;
    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;

    // Memory viewer
    QGroupBox *m_memoryGroup;
    QTableWidget *m_memoryTable;
    QLineEdit *m_addressInput;
    QPushButton *m_refreshButton;
    QPushButton *m_gotoButton;

    // Memory search
    QGroupBox *m_searchGroup;
    QLineEdit *m_searchValue;
    QComboBox *m_searchType;
    QComboBox *m_compareType;
    QPushButton *m_searchButton;
    QPushButton *m_nextScanButton;
    QPushButton *m_resetButton;
    QLabel *m_resultsLabel;

    // Cheat manager
    QGroupBox *m_cheatGroup;
    QTableWidget *m_cheatTable;
    QPushButton *m_addCheatButton;
    QPushButton *m_removeCheatButton;
    QPushButton *m_toggleFreezeButton;

    // Cheat editor
    QGroupBox *m_editorGroup;
    QLineEdit *m_cheatAddress;
    QLineEdit *m_cheatValue;
    QLineEdit *m_cheatDescription;
    QComboBox *m_cheatType;
    QCheckBox *m_freezeCheckbox;

    // Status and info
    QTextEdit *m_logOutput;
    QLabel *m_statusLabel;

    // Timer for auto-refresh
    QTimer *m_refreshTimer;

    // Data structures
    struct CheatEntry {
        QString address;
        QString value;
        QString description;
        QString type;
        bool frozen;
        bool enabled;
    };

    struct MemoryEntry {
        QString address;
        QString value;
        QString oldValue;
        bool changed;
    };

    QList<CheatEntry> m_cheats;
    QList<MemoryEntry> m_memoryEntries;
    QList<QString> m_searchResults;
    std::shared_ptr<QGBA::CoreController> m_controller;

};

#endif // CHEATENGINE_H
