#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <optional>

#include "../core/chessGame.hpp"

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const std::string& path);
    void close();

    int insertImportSource(const std::string& filename);
    bool updateImportSourceGameCount(int sourceId, int count);

    bool insertGame(const ChessGame& game);

    bool beginTransaction();
    bool commitTransaction();
    void rollbackTransaction();

    std::vector<ChessGame> getAllGames();

    enum class PlayerColor {
    White,
    Black
};

    std::vector<ChessGame> searchGames(
    const std::string& player,
    PlayerColor color,
    std::optional<std::string> date = std::nullopt
);

private:
    sqlite3* m_db = nullptr;

    bool createTables();
};
