
#include "repertoire_tree.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

// Find an existing child with the given UCI move.
// If none exists, create it.
RepertoireNode& getOrCreateChild(
    RepertoireNode& parent,
    const std::string& moveUci
) {
    auto it = std::find_if(
        parent.children.begin(),
        parent.children.end(),
        [&](const RepertoireNode& child) {
            return child.moveUci == moveUci;
        }
    );

    if (it != parent.children.end()) {
        return *it;
    }

    RepertoireNode child;
    child.moveUci = moveUci;

    parent.children.push_back(std::move(child));

    return parent.children.back();
}

// Sort all continuations by frequency.
// The most frequently played continuation comes first.
void sortTree(RepertoireNode& node) {
    std::sort(
        node.children.begin(),
        node.children.end(),
        [](const RepertoireNode& a, const RepertoireNode& b) {
            if (a.gameCount != b.gameCount) {
                return a.gameCount > b.gameCount;
            }

            return a.moveUci < b.moveUci;
        }
    );

    for (auto& child : node.children) {
        sortTree(child);
    }
}

} // namespace

std::vector<RepertoireTree> RepertoireBuilder::build(
    const std::vector<ChessGame>& games,
    int maxFullMove
) {
    if (maxFullMove <= 0) {
        throw std::invalid_argument(
            "Maximum full move must be greater than zero."
        );
    }

    std::vector<RepertoireTree> trees;

    const std::size_t maxPlies =
        static_cast<std::size_t>(maxFullMove) * 2;

    for (const ChessGame& game : games) {

        // Find or create a tree for this starting position.
        auto treeIt = std::find_if(
            trees.begin(),
            trees.end(),
            [&](const RepertoireTree& tree) {
                return tree.startingFen == game.startingFen;
            }
        );

        if (treeIt == trees.end()) {
            RepertoireTree newTree;
            newTree.startingFen = game.startingFen;

            trees.push_back(std::move(newTree));

            treeIt = std::prev(trees.end());
        }

        RepertoireNode* current = &treeIt->root;

        // Count this game at the root.
        current->gameCount++;

        // Never process more than maxFullMove * 2 plies.
        const std::size_t moveCount = std::min(
            game.movesUci.size(),
            maxPlies
        );

        for (std::size_t i = 0; i < moveCount; ++i) {

            const std::string& moveUci = game.movesUci[i];

            // Find or create the next node.
            RepertoireNode& child =
                getOrCreateChild(*current, moveUci);

            // This game supports this continuation.
            child.gameCount++;

            current = &child;
        }
    }

    // Put the most popular continuation first everywhere.
    for (auto& tree : trees) {
        sortTree(tree.root);
    }

    return trees;
}
