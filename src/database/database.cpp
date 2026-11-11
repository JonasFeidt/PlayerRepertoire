#include "database.hpp"

#include <iostream>
#include <sstream>
#include <optional>
#include <vector>

namespace {

// Safely read a text column from an SQLite result row.
std::string getColumnText(sqlite3_stmt* stmt, int column) {
    const unsigned char* value = sqlite3_column_text(stmt, column);

    if (value == nullptr) {
        return "";
    }

    return reinterpret_cast<const char*>(value);
}

// Reconstruct a ChessGame from the current SQLite result row.
ChessGame readGame(sqlite3_stmt* stmt) {
    ChessGame game;

    game.id = sqlite3_column_int(stmt, 0);
    game.importSourceId = sqlite3_column_int(stmt, 1);

    auto& metadata = game.metadata;

    metadata.event = getColumnText(stmt, 2);
    metadata.site = getColumnText(stmt, 3);
    metadata.date = getColumnText(stmt, 4);
    metadata.round = getColumnText(stmt, 5);

    metadata.white = getColumnText(stmt, 6);
    metadata.black = getColumnText(stmt, 7);
    metadata.result = getColumnText(stmt, 8);

    if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
        metadata.whiteElo = sqlite3_column_int(stmt, 9);
    }

    if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
        metadata.blackElo = sqlite3_column_int(stmt, 10);
    }

    metadata.eco = getColumnText(stmt, 11);
    metadata.opening = getColumnText(stmt, 12);
    metadata.timeControl = getColumnText(stmt, 13);

    game.startingFen = getColumnText(stmt, 14);
    game.originalPgn = getColumnText(stmt, 16);

    // Convert the stored space-separated UCI moves back into a vector.
    std::istringstream moves(getColumnText(stmt, 15));

    std::string move;

    while (moves >> move) {
        game.movesUci.push_back(move);
    }

    return game;
}

} // namespace

Database::~Database() {
    close();
}

bool Database::open(const std::string& path) {
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK) {
        std::cerr << "Could not open database: "
                  << (m_db ? sqlite3_errmsg(m_db) : "Unknown error")
                  << '\n';

        close();
        return false;
    }

    // Enable foreign key constraints.
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    return createTables();
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::createTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS import_sources (
            id INTEGER PRIMARY KEY,
            filename TEXT NOT NULL,
            imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            game_count INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS games (
            id INTEGER PRIMARY KEY,
            import_source_id INTEGER,

            event TEXT,
            site TEXT,
            date TEXT,
            round TEXT,

            white TEXT,
            black TEXT,
            result TEXT,

            white_elo INTEGER,
            black_elo INTEGER,

            eco TEXT,
            opening TEXT,
            time_control TEXT,

            starting_fen TEXT,
            moves_uci TEXT,
            original_pgn TEXT,

            content_hash TEXT UNIQUE,

            FOREIGN KEY(import_source_id)
                REFERENCES import_sources(id)
        );

        CREATE INDEX IF NOT EXISTS idx_games_white ON games(white);
        CREATE INDEX IF NOT EXISTS idx_games_black ON games(black);
        CREATE INDEX IF NOT EXISTS idx_games_event ON games(event);
        CREATE INDEX IF NOT EXISTS idx_games_date ON games(date);
    )";

    char* errorMessage = nullptr;

    const int result = sqlite3_exec(
        m_db, sql, nullptr, nullptr, &errorMessage
    );

    if (result != SQLITE_OK) {
        std::cerr << "Table creation failed: "
                  << errorMessage << '\n';

        sqlite3_free(errorMessage);
        return false;
    }

    return true;
}

bool Database::beginTransaction() {
    return sqlite3_exec(
        m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr
    ) == SQLITE_OK;
}

bool Database::commitTransaction() {
    return sqlite3_exec(
        m_db, "COMMIT;", nullptr, nullptr, nullptr
    ) == SQLITE_OK;
}

void Database::rollbackTransaction() {
    sqlite3_exec(
        m_db, "ROLLBACK;", nullptr, nullptr, nullptr
    );
}

int Database::insertImportSource(const std::string& filename) {
    const char* sql = R"(
        INSERT INTO import_sources (filename)
        VALUES (?);
    )";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(
        stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT
    );

    const int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        return -1;
    }

    return static_cast<int>(sqlite3_last_insert_rowid(m_db));
}

bool Database::updateImportSourceGameCount(int sourceId, int count) {
    const char* sql = R"(
        UPDATE import_sources
        SET game_count = ?
        WHERE id = ?;
    )";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, count);
    sqlite3_bind_int(stmt, 2, sourceId);

    const int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

bool Database::insertGame(const ChessGame& game) {
    const char* sql = R"(
        INSERT INTO games (
            import_source_id,
            event, site, date, round,
            white, black, result,
            white_elo, black_elo,
            eco, opening, time_control,
            starting_fen, moves_uci, original_pgn
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Could not prepare game insertion: "
                  << sqlite3_errmsg(m_db) << '\n';
        return false;
    }

    const auto& metadata = game.metadata;

    // Convert the UCI move vector into one space-separated string.
    std::ostringstream moves;

    for (std::size_t i = 0; i < game.movesUci.size(); ++i) {
        if (i > 0) {
            moves << ' ';
        }

        moves << game.movesUci[i];
    }

    int index = 1;

    sqlite3_bind_int(stmt, index++, game.importSourceId);

    auto bindText = [&](const std::string& value) {
        sqlite3_bind_text(
            stmt, index++, value.c_str(), -1, SQLITE_TRANSIENT
        );
    };

    auto bindOptionalInt = [&](const std::optional<int>& value) {
        if (value) {
            sqlite3_bind_int(stmt, index++, *value);
        } else {
            sqlite3_bind_null(stmt, index++);
        }
    };

    bindText(metadata.event);
    bindText(metadata.site);
    bindText(metadata.date);
    bindText(metadata.round);

    bindText(metadata.white);
    bindText(metadata.black);
    bindText(metadata.result);

    bindOptionalInt(metadata.whiteElo);
    bindOptionalInt(metadata.blackElo);

    bindText(metadata.eco);
    bindText(metadata.opening);
    bindText(metadata.timeControl);

    bindText(game.startingFen);
    bindText(moves.str());
    bindText(game.originalPgn);

    const int result = sqlite3_step(stmt);

    if (result != SQLITE_DONE) {
        std::cerr << "Could not insert game: "
                  << sqlite3_errmsg(m_db) << '\n';
    }

    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

std::vector<ChessGame> Database::getAllGames() {
    std::vector<ChessGame> games;

    if (!m_db) {
        return games;
    }

    const char* sql = R"(
        SELECT
            id, import_source_id,
            event, site, date, round,
            white, black, result,
            white_elo, black_elo,
            eco, opening, time_control,
            starting_fen, moves_uci, original_pgn
        FROM games
        ORDER BY id;
    )";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Could not prepare game retrieval: "
                  << sqlite3_errmsg(m_db) << '\n';
        return games;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        games.push_back(readGame(stmt));
    }

    sqlite3_finalize(stmt);

    return games;
}

std::vector<ChessGame> Database::searchGames(
    const std::string& player,
    PlayerColor color,
    std::optional<std::string> date
) {
    std::vector<ChessGame> games;

    if (!m_db) {
        return games;
    }

    // Determine which player column to search.
    const char* playerColumn = nullptr;

    switch (color) {
        case PlayerColor::White:
            playerColumn = "white";
            break;

        case PlayerColor::Black:
            playerColumn = "black";
            break;

        default:
            return games;
    }

    // Construct the query.
    std::string sql = R"(
        SELECT
            id, import_source_id,
            event, site, date, round,
            white, black, result,
            white_elo, black_elo,
            eco, opening, time_control,
            starting_fen, moves_uci, original_pgn
        FROM games
        WHERE )";

    sql += playerColumn;
    sql += " = ? COLLATE NOCASE";

    if (date.has_value()) {
        sql += " AND date >= ?";
    }

    sql += " ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Could not prepare game search: "
                  << sqlite3_errmsg(m_db) << '\n';
        return games;
    }

    // Bind the player name.
    sqlite3_bind_text(
        stmt, 1, player.c_str(), -1, SQLITE_TRANSIENT
    );

    // Bind the date only when one was provided.
    if (date.has_value()) {
        sqlite3_bind_text(
            stmt, 2, date->c_str(), -1, SQLITE_TRANSIENT
        );
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        games.push_back(readGame(stmt));
    }

    sqlite3_finalize(stmt);

    return games;
}