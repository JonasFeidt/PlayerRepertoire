
#include "repertoire_tree.hpp"

#include <chess.hpp>

#include <sstream>

namespace {

// Convert every child using the position at its parent.
bool convertChildren(
    RepertoireNode& parent,
    const chess::Board& board,
    std::string& error
) {
    for (auto& child : parent.children) {
        try {
            // Interpret the UCI move in the current position.
            const chess::Move move =
                chess::uci::uciToMove(board, child.moveUci);

            if (move == chess::Move::NO_MOVE ||
                !board.isLegal(move)) {
                error = "Invalid or illegal UCI move: " +
                        child.moveUci;
                return false;
            }

            // Generate SAN before making the move.
            child.moveSan = chess::uci::moveToSan(board, move);

            // Each variation needs its own board state.
            chess::Board nextBoard = board;
            nextBoard.makeMove(move);

            // Continue recursively through this branch.
            if (!convertChildren(child, nextBoard, error)) {
                return false;
            }
        }
        catch (const std::exception& exception) {
            error = "SAN conversion failed for " +
                    child.moveUci + ": " + exception.what();
            return false;
        }
    }

    return true;
}

// Print the tree recursively.
void printNode(
    const RepertoireNode& node,
    const chess::Board& board,
    std::ostringstream& output,
    int depth
) {
    for (const auto& child : node.children) {
        const chess::Move move =
            chess::uci::uciToMove(board, child.moveUci);

        // Indentation shows the branch depth.
        output << std::string(depth * 2, ' ');

        // Add the appropriate move number.
        output << board.fullMoveNumber();

        if (board.sideToMove() == chess::Color::WHITE) {
            output << ". ";
        } else {
            output << "... ";
        }

        output << child.moveSan
               << " (" << child.gameCount << " games)\n";

        chess::Board nextBoard = board;
        nextBoard.makeMove(move);

        printNode(child, nextBoard, output, depth + 1);
    }
}

} // namespace

bool RepertoireNotation::convertToSan(
    RepertoireTree& tree,
    std::string& error
) {
    error.clear();

    chess::Board board;

    // Use the game's starting position if it has a custom FEN.
    if (!tree.startingFen.empty()) {
        if (!board.setFen(tree.startingFen)) {
            error = "Invalid starting FEN: " + tree.startingFen;
            return false;
        }
    }

    return convertChildren(tree.root, board, error);
}

std::string RepertoireNotation::toText(
    const RepertoireTree& tree
) {
    chess::Board board;

    if (!tree.startingFen.empty() &&
        !board.setFen(tree.startingFen)) {
        return "Invalid starting FEN.\n";
    }

    std::ostringstream output;

    output << "Repertoire (" << tree.root.gameCount
           << " games)\n";

    output << "-------------------------\n";

    printNode(tree.root, board, output, 0);

    return output.str();
}
