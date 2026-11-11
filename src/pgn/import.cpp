#include "import.hpp"

#include <chess.hpp>

#include <fstream>
#include <optional>
#include <string_view>

namespace {

std::optional<int> parseElo(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }

    try {
        return std::stoi(std::string(value));
    } catch (...) {
        return std::nullopt;
    }
}

class GameVisitor : public chess::pgn::Visitor {
public:
    std::vector<ChessGame> games;
    std::vector<std::string> errors;

    void startPgn() override {
        currentGame = ChessGame{};
        board = chess::Board{};
        invalidGame = false;
        gameError.clear();
    }

    void header(
        std::string_view key,
        std::string_view value
    ) override {
        auto& metadata = currentGame.metadata;

        if (key == "Event") {
            metadata.event = value;
        } else if (key == "Site") {
            metadata.site = value;
        } else if (key == "Date") {
            metadata.date = value;
        } else if (key == "Round") {
            metadata.round = value;
        } else if (key == "White") {
            metadata.white = value;
        } else if (key == "Black") {
            metadata.black = value;
        } else if (key == "Result") {
            metadata.result = value;
        } else if (key == "WhiteElo") {
            metadata.whiteElo = parseElo(value);
        } else if (key == "BlackElo") {
            metadata.blackElo = parseElo(value);
        } else if (key == "ECO") {
            metadata.eco = value;
        } else if (key == "Opening") {
            metadata.opening = value;
        } else if (key == "TimeControl") {
            metadata.timeControl = value;
        } else if (key == "FEN") {
            currentGame.startingFen = value;

            if (!board.setFen(value)) {
                invalidate("Invalid starting FEN");
            }
        }
    }

    void startMoves() override {
        // Headers have been read.
        // The board is already initialized.
    }

    void move(
        std::string_view san,
        std::string_view comment
    ) override {
        (void)comment;

        if (san.empty() || invalidGame) {
            return;
        }

        try {
            const chess::Move move =
                chess::uci::parseSan(board, san);

            if (move == chess::Move::NO_MOVE ||
                !board.isLegal(move)) {
                invalidate(
                    "Illegal or unrecognized move: " +
                    std::string(san)
                );
                return;
            }

            currentGame.movesUci.push_back(
                chess::uci::moveToUci(
                    move,
                    board.chess960()
                )
            );

            board.makeMove(move);
        } catch (const std::exception& exception) {
            invalidate(
                "Could not parse move '" +
                std::string(san) +
                "': " + exception.what()
            );
        }
    }

    void endPgn() override {
        if (invalidGame) {
            errors.push_back(gameError);
            return;
        }

        games.push_back(std::move(currentGame));
    }

private:
    ChessGame currentGame;
    chess::Board board;

    bool invalidGame = false;
    std::string gameError;

    void invalidate(const std::string& message) {
        invalidGame = true;
        gameError = message;
    }
};

} // namespace

PGNImportResult PGNImporter::importFile(
    const std::filesystem::path& path
) {
    PGNImportResult result;

    std::ifstream input(path);

    if (!input.is_open()) {
        result.errors.push_back(
            "Could not open file: " + path.string()
        );
        return result;
    }

    GameVisitor visitor;

    chess::pgn::StreamParser parser(input);
    const auto parserError = parser.readGames(visitor);

    result.games = std::move(visitor.games);
    result.errors = std::move(visitor.errors);

    if (parserError) {
        result.errors.push_back(
            "PGN parser error code: " +
            std::to_string(
                static_cast<int>(parserError.code())
            )
        );
    } else {
        result.success = true;
    }

    return result;
}