
#pragma once

#include <QMainWindow>

#include "repertoire/repertoire_tree.hpp"

#include <vector>

class Database;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QSpinBox;
class QTreeWidget;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Database& database, QWidget* parent = nullptr);

private:
    Database& m_database;

    QLineEdit* m_playerInput = nullptr;
    QComboBox* m_colorInput = nullptr;
    QCheckBox* m_useDateFilter = nullptr;
    QSpinBox* m_yearInput = nullptr;
    QComboBox* m_monthInput = nullptr;
    QSpinBox* m_maxMovesInput = nullptr;

    QComboBox* m_startingPositionInput = nullptr;
    QTreeWidget* m_repertoireView = nullptr;
    QLabel* m_statusLabel = nullptr;

    std::vector<RepertoireTree> m_trees;

    void generateRepertoire();
    void updateRepertoireView();
    void exportRepertoire();
    void importPGN();
};
