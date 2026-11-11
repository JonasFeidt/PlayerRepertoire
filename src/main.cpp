
#include <QApplication>

#include "mainWindow.hpp"
#include "database/database.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    Database database;

    if (!database.open("chess_collection.db")) {
        std::cerr << "Failed to initialize database.\n";
        return 1;
    }

    MainWindow window(database);
    window.show();

    return app.exec();
}
