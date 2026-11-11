
#include "pgn_exporter.hpp"
#include "repertoire/repertoire_tree.hpp"

#include <chess.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

// Escape special characters in PGN tag values.
std::string escapeTag(const std::string& value) {
    std::string result;

    for (char c : value) {
        if (c == '\\' || c == '"') {
            result += '\\';
        }

        result += c;
    }

    return result;
}

void writeTag(
    std::ostringstream& output,
    const std::string& name,
    const std::string& value
) {
    output << '[' << name << " \""
           << escapeTag(value) << "\"]\n";
}

// Write a single move with its appropriate move number.
void writeMove(
    const RepertoireNode& node,
    const chess::Board& board,
    std::ostringstream& output,
    bool firstMove
) {
    if (node.moveSan.empty()) {
        throw std::runtime_error(
            "Missing SAN notation for move: " + node.moveUci
        );
    }

    if (board.sideToMove() == chess::Color::WHITE) {
        output << board.fullMoveNumber() << ". ";
    }
    else if (firstMove) {
        // A variation can begin with Black's move.
        output << board.fullMoveNumber() << "... ";
    }

    output << node.moveSan;
}

// Apply a move to a copied board.
chess::Board makeNextBoard(
    const RepertoireNode& node,
    const chess::Board& board
) {
    const chess::Move move =
        chess::uci::uciToMove(board, node.moveUci);

    if (move == chess::Move::NO_MOVE || !board.isLegal(move)) {
        throw std::runtime_error(
            "Invalid or illegal move: " + node.moveUci
        );
    }

    chess::Board nextBoard = board;
    nextBoard.makeMove(move);

    return nextBoard;
}

// Forward declaration because the two functions call each other.
void writeMainLine(
    const RepertoireNode& parent,
    const chess::Board& board,
    std::ostringstream& output,
    bool firstMove = true
);

// Write the complete continuation of one alternative branch.
void writeBranch(
    const RepertoireNode& node,
    const chess::Board& board,
    std::ostringstream& output
) {
    writeMove(node, board, output, true);

    const chess::Board nextBoard = makeNextBoard(node, board);

    // The alternative's first move has already been written.
    writeMainLine(node, nextBoard, output, false);
}

// Write the main line and all its alternatives.
void writeMainLine(
    const RepertoireNode& parent,
    const chess::Board& board,
    std::ostringstream& output,
    bool firstMove
) {
    const RepertoireNode* current = &parent;
    chess::Board currentBoard = board;

    while (!current->children.empty()) {
        const RepertoireNode& mainMove = current->children[0];

        if (!firstMove) {
            output << ' ';
        }

        writeMove(mainMove, currentBoard, output, firstMove);

        // Write alternative moves from the same position.
        for (std::size_t i = 1;
             i < current->children.size();
             ++i) {

            output << " (";

            writeBranch(
                current->children[i],
                currentBoard,
                output
            );

            output << ')';
        }

        // Continue along the most popular move.
        currentBoard = makeNextBoard(mainMove, currentBoard);

        current = &mainMove;
        firstMove = false;
    }
}


} // namespace

std::string PGNExporter::toPgn(
    const RepertoireTree& tree,
    const PGNExportOptions& options,
    std::string& error
) {
    error.clear();

    try {
        chess::Board board;

        // Restore the correct starting position.
        if (!tree.startingFen.empty()) {
            if (!board.setFen(tree.startingFen)) {
                error = "Invalid starting FEN: " + tree.startingFen;
                return "";
            }
        }

        std::ostringstream output;

        // PGN headers.
        writeTag(output, "Event", options.event);
        writeTag(output, "Site", options.site);
        writeTag(output, "Date", options.date);
        writeTag(output, "Round", options.round);
        writeTag(output, "White", options.white);
        writeTag(output, "Black", options.black);
        writeTag(output, "Result", options.result);

        // Custom starting positions require these two tags.
        if (!tree.startingFen.empty()) {
            writeTag(output, "SetUp", "1");
            writeTag(output, "FEN", tree.startingFen);
        }

        output << '\n';

        // Write the actual repertoire.
        writeMainLine(tree.root, board, output);

        // Terminate the PGN movetext.
        output << ' ' << options.result << '\n';

        return output.str();
    }
    catch (const std::exception& exception) {
        error = exception.what();
        return "";
    }
}

bool PGNExporter::writeToFile(
    const RepertoireTree& tree,
    const PGNExportOptions& options,
    const std::string& filename,
    std::string& error
) {
    const std::string pgn = toPgn(tree, options, error);

    if (!error.empty()) {
        return false;
    }

    std::ofstream file(filename);

    if (!file.is_open()) {
        error = "Could not open output file: " + filename;
        return false;
    }

    file << pgn;

    if (!file.good()) {
        error = "Could not write PGN file: " + filename;
        return false;
    }

    return true;
}
