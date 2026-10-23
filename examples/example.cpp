// A tour of chessjs.hpp: playing moves, listing legal moves, game state, undo and PGN.
#include <chessjs.hpp>

#include <iostream>

int main()
{
    chessjs::Chess chess;

    // Moves in Standard Algebraic Notation, as from/to squares, or (permissively) in long algebraic notation.
    chess.move("e4");
    chess.move(chessjs::MoveInput{"e7", "e5"});
    chess.move("g1f3");
    chess.move("Nc6");

    std::cout << chess.ascii() << "\n\n";
    std::cout << "FEN: " << chess.fen() << "\n";
    std::cout << "White to move: " << (chess.turn() == chessjs::WHITE ? "yes" : "no") << "\n";

    // Every legal move, in SAN.
    std::cout << "Legal moves:";
    for (const std::string& san : chess.moves()) {
        std::cout << ' ' << san;
    }
    std::cout << "\n";

    // Verbose moves carry the squares, the pieces and the position before and after.
    for (const chessjs::Move& move : chess.movesVerbose({"f1"})) {
        std::cout << "  " << move.san << ": " << move.from << " -> " << move.to << (move.isCapture() ? " (capture)" : "")
                  << "\n";
    }

    // Illegal moves throw chessjs::Error.
    try {
        chess.move("Ke3");
    } catch (const chessjs::Error& error) {
        std::cout << error.what() << "\n";
    }

    // Undo returns the move it took back.
    if (const auto undone = chess.undo()) {
        std::cout << "Undid " << undone->san << "\n";
    }

    // Load a game from PGN, then check how it ended.
    chess.loadPgn(R"([White "Me"]
[Black "Also me"]

1. f3 e5 2. g4 Qh4# 0-1)");
    std::cout << "Checkmate: " << (chess.isCheckmate() ? "yes" : "no") << ", game over: "
              << (chess.isGameOver() ? "yes" : "no") << "\n";

    // Headers, comments and the PGN back out.
    chess.setHeader("Event", "Fool's mate");
    chess.setComment("Ouch.");
    std::cout << chess.pgn() << "\n";
}
