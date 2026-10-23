/*
 * chessjs.hpp 1.0.0 - a header-only C++17 port of chess.js
 * https://github.com/RatPrez/chessjs-cpp
 *
 * Ported from https://github.com/jhlywa/chess.js (src/chess.ts and
 * src/pgn.peggy) at commit d43e668, which is newer than the chess.js 1.4.0
 * npm release (it adds NAGs and Move::isCheck()).
 *
 * The public API mirrors chess.js. JavaScript option objects become small
 * option structs, `moves({ verbose: true })` / `history({ verbose: true })`
 * become movesVerbose() / historyVerbose(), and methods that throw in
 * chess.js throw chessjs::Error. Deprecated chess.js methods (header,
 * deleteComment, deleteComments) are not ported.
 *
 * Intentional differences from chess.js (each fixes a chess.js bug):
 *  - put()/remove() only clear the en passant square when it stops being
 *    capturable (chess.js always clears it due to an undefined/null mix-up).
 *  - Null moves update the position hash, so undoing one restores it.
 *  - setCastlingRights() keeps the position hash in sync with the rights.
 * Together these keep hash() (and threefold repetition) consistent with the
 * position. Also, a PGN NAG above 999999 throws chessjs::Error, since a NAG
 * is an int here (the PGN standard only defines NAGs 0 to 255). Everything
 * else is verified to match chess.js output exactly.
 *
 * Usage:
 *     chessjs::Chess chess;
 *     chess.move("e4");
 *     chess.move(chessjs::MoveInput{"e7", "e5"});
 *     for (const auto& move : chess.movesVerbose()) { ... }
 *
 * ----------------------------------------------------------------------------
 * License (BSD-2-Clause, the same as chess.js):
 *
 * Copyright (c) 2026, Richard Saville (C++ port)
 * Copyright (c) 2025, Jeff Hlywa (jhlywa@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chessjs {

// ---------------------------------------------------------------------------
// Public types and constants
// ---------------------------------------------------------------------------

enum Color : char { WHITE = 'w', BLACK = 'b' };

enum PieceSymbol : char {
    PAWN = 'p',
    KNIGHT = 'n',
    BISHOP = 'b',
    ROOK = 'r',
    QUEEN = 'q',
    KING = 'k',
};

using Square = std::string;
using NAG = int;

inline constexpr const char* DEFAULT_POSITION = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// clang-format off
inline constexpr std::array<std::string_view, 64> SQUARES = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};
// clang-format on

inline constexpr std::array<std::string_view, 6> SUFFIX_LIST = {"!", "?", "!!", "!?", "?!", "??"};

// These are required, according to the PGN spec.
inline const std::vector<std::pair<std::string, std::string>> SEVEN_TAG_ROSTER = {
    {"Event", "?"}, {"Site", "?"}, {"Date", "????.??.??"}, {"Round", "?"},
    {"White", "?"}, {"Black", "?"}, {"Result", "*"},
};

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Piece {
    Color color;
    PieceSymbol type;

    friend bool operator==(const Piece& a, const Piece& b) { return a.color == b.color && a.type == b.type; }
    friend bool operator!=(const Piece& a, const Piece& b) { return !(a == b); }
};

struct BoardSquare {
    Square square;
    PieceSymbol type;
    Color color;
};

class Move {
public:
    Color color = WHITE;
    Square from;
    Square to;
    PieceSymbol piece = PAWN;
    std::optional<PieceSymbol> captured;
    std::optional<PieceSymbol> promotion;

    // Deprecated in chess.js; prefer the is*() descriptors below.
    std::string flags;

    std::string san;
    std::string lan;
    std::string before;
    std::string after;

    bool isCapture() const { return flags.find('c') != std::string::npos; }
    bool isPromotion() const { return flags.find('p') != std::string::npos; }
    bool isEnPassant() const { return flags.find('e') != std::string::npos; }
    bool isKingsideCastle() const { return flags.find('k') != std::string::npos; }
    bool isQueensideCastle() const { return flags.find('q') != std::string::npos; }
    bool isBigPawn() const { return flags.find('b') != std::string::npos; }
    bool isNullMove() const { return flags.find('-') != std::string::npos; }
    bool isCheck() const { return san.find_first_of("+#") != std::string::npos; }
};

struct MoveInput {
    Square from;
    Square to;
    std::optional<PieceSymbol> promotion;
};

struct LoadOptions {
    bool skipValidation = false;
    bool preserveHeaders = false;
};

struct ClearOptions {
    bool preserveHeaders = false;
};

struct FenOptions {
    bool forceEnpassantSquare = false;
};

struct MovesOptions {
    std::optional<Square> square;
    std::optional<PieceSymbol> piece;
};

struct MoveOptions {
    bool strict = false;
};

struct PgnOptions {
    std::string newline = "\n";
    int maxWidth = 0;
};

struct LoadPgnOptions {
    bool strict = false;
    // A regular expression (ECMAScript syntax) matching line breaks.
    std::string newlineChar = "\r?\n";
};

struct CastlingRights {
    bool king = false;
    bool queen = false;
};

struct CastlingRightsUpdate {
    std::optional<bool> king;
    std::optional<bool> queen;
};

struct CommentEntry {
    std::string fen;
    std::optional<std::string> comment;
    std::optional<std::string> suffixAnnotation;
    std::vector<NAG> nags;
};

struct FenComment {
    std::string fen;
    std::string comment;
};

struct FenValidation {
    bool ok = true;
    std::string error;
};

using Headers = std::vector<std::pair<std::string, std::string>>;

// Converts a NAG (Numeric Annotation Glyph) to its UTF-8 symbol, if it has one.
inline std::optional<std::string> nagToGlyph(NAG nag)
{
    switch (nag) {
    case 7: return "\xE2\x96\xA1";                  // Only move
    case 22: return "\xE2\xA8\x80";                 // Zugzwang
    case 10: return "=";                            // Equal position
    case 13: return "\xE2\x88\x9E";                 // Unclear position
    case 14: return "\xE2\xA9\xB2";                 // White is slightly better
    case 15: return "\xE2\xA9\xB1";                 // Black is slightly better
    case 16: return "\xC2\xB1";                     // White is better
    case 17: return "\xE2\x88\x93";                 // Black is better
    case 18: return "+\xE2\x88\x92";                // White is winning
    case 19: return "-+";                           // Black is winning
    case 146: return "N";                           // Novelty
    case 32: return "\xE2\x86\x91\xE2\x86\x91";     // Development
    case 36: return "\xE2\x86\x91";                 // Initiative
    case 40: return "\xE2\x86\x92";                 // Attack
    case 132: return "\xE2\x87\x86";                // Counterplay
    case 138: return "\xE2\x8A\x95";                // Time trouble
    case 44: return "=\xE2\x88\x9E";                // With compensation
    case 140: return "\xE2\x88\x86";                // With the idea
    default: return std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

namespace detail {

inline constexpr int EMPTY = -1;

namespace bits {
inline constexpr int NORMAL = 1;
inline constexpr int CAPTURE = 2;
inline constexpr int BIG_PAWN = 4;
inline constexpr int EP_CAPTURE = 8;
inline constexpr int PROMOTION = 16;
inline constexpr int KSIDE_CASTLE = 32;
inline constexpr int QSIDE_CASTLE = 64;
inline constexpr int NULL_MOVE = 128;
} // namespace bits

inline constexpr std::array<std::pair<int, char>, 8> FLAG_CHARS = {{
    {bits::NORMAL, 'n'},
    {bits::CAPTURE, 'c'},
    {bits::BIG_PAWN, 'b'},
    {bits::EP_CAPTURE, 'e'},
    {bits::PROMOTION, 'p'},
    {bits::KSIDE_CASTLE, 'k'},
    {bits::QSIDE_CASTLE, 'q'},
    {bits::NULL_MOVE, '-'},
}};

/*
 * The board uses the 0x88 representation: an 8x16 array where bitwise AND
 * with 0x88 is a cheap off-board test, and the difference between two
 * squares uniquely identifies direction and distance (see ATTACKS / RAYS).
 */
inline constexpr int SQ_A8 = 0;
inline constexpr int SQ_H8 = 7;
inline constexpr int SQ_E8 = 4;
inline constexpr int SQ_A1 = 112;
inline constexpr int SQ_E1 = 116;
inline constexpr int SQ_H1 = 119;

inline constexpr int RANK_1 = 7;
inline constexpr int RANK_2 = 6;
inline constexpr int RANK_7 = 1;
inline constexpr int RANK_8 = 0;

inline constexpr std::array<int, 4> PAWN_OFFSETS_BLACK = {16, 32, 17, 15};
inline constexpr std::array<int, 4> PAWN_OFFSETS_WHITE = {-16, -32, -17, -15};

inline constexpr std::array<int, 8> KNIGHT_OFFSETS = {-18, -33, -31, -14, 18, 33, 31, 14};
inline constexpr std::array<int, 4> BISHOP_OFFSETS = {-17, -15, 17, 15};
inline constexpr std::array<int, 4> ROOK_OFFSETS = {-16, 1, 16, -1};
inline constexpr std::array<int, 8> KING_OFFSETS = {-17, -16, -15, 1, 17, 16, 15, -1};

// clang-format off
inline constexpr std::array<int, 240> ATTACKS = {
    20, 0, 0, 0, 0, 0, 0, 24,  0, 0, 0, 0, 0, 0,20, 0,
     0,20, 0, 0, 0, 0, 0, 24,  0, 0, 0, 0, 0,20, 0, 0,
     0, 0,20, 0, 0, 0, 0, 24,  0, 0, 0, 0,20, 0, 0, 0,
     0, 0, 0,20, 0, 0, 0, 24,  0, 0, 0,20, 0, 0, 0, 0,
     0, 0, 0, 0,20, 0, 0, 24,  0, 0,20, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,20, 2, 24,  2,20, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 2,53, 56, 53, 2, 0, 0, 0, 0, 0, 0,
    24,24,24,24,24,24,56,  0, 56,24,24,24,24,24,24, 0,
     0, 0, 0, 0, 0, 2,53, 56, 53, 2, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,20, 2, 24,  2,20, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0,20, 0, 0, 24,  0, 0,20, 0, 0, 0, 0, 0,
     0, 0, 0,20, 0, 0, 0, 24,  0, 0, 0,20, 0, 0, 0, 0,
     0, 0,20, 0, 0, 0, 0, 24,  0, 0, 0, 0,20, 0, 0, 0,
     0,20, 0, 0, 0, 0, 0, 24,  0, 0, 0, 0, 0,20, 0, 0,
    20, 0, 0, 0, 0, 0, 0, 24,  0, 0, 0, 0, 0, 0,20,
};

inline constexpr std::array<int, 240> RAYS = {
     17,  0,  0,  0,  0,  0,  0, 16,  0,  0,  0,  0,  0,  0, 15, 0,
      0, 17,  0,  0,  0,  0,  0, 16,  0,  0,  0,  0,  0, 15,  0, 0,
      0,  0, 17,  0,  0,  0,  0, 16,  0,  0,  0,  0, 15,  0,  0, 0,
      0,  0,  0, 17,  0,  0,  0, 16,  0,  0,  0, 15,  0,  0,  0, 0,
      0,  0,  0,  0, 17,  0,  0, 16,  0,  0, 15,  0,  0,  0,  0, 0,
      0,  0,  0,  0,  0, 17,  0, 16,  0, 15,  0,  0,  0,  0,  0, 0,
      0,  0,  0,  0,  0,  0, 17, 16, 15,  0,  0,  0,  0,  0,  0, 0,
      1,  1,  1,  1,  1,  1,  1,  0, -1, -1,  -1,-1, -1, -1, -1, 0,
      0,  0,  0,  0,  0,  0,-15,-16,-17,  0,  0,  0,  0,  0,  0, 0,
      0,  0,  0,  0,  0,-15,  0,-16,  0,-17,  0,  0,  0,  0,  0, 0,
      0,  0,  0,  0,-15,  0,  0,-16,  0,  0,-17,  0,  0,  0,  0, 0,
      0,  0,  0,-15,  0,  0,  0,-16,  0,  0,  0,-17,  0,  0,  0, 0,
      0,  0,-15,  0,  0,  0,  0,-16,  0,  0,  0,  0,-17,  0,  0, 0,
      0,-15,  0,  0,  0,  0,  0,-16,  0,  0,  0,  0,  0,-17,  0, 0,
    -15,  0,  0,  0,  0,  0,  0,-16,  0,  0,  0,  0,  0,  0,-17,
};
// clang-format on

inline constexpr std::array<PieceSymbol, 4> PROMOTIONS = {KNIGHT, BISHOP, ROOK, QUEEN};

inline constexpr const char* SAN_NULLMOVE = "--";

inline int rankOf(int square) { return square >> 4; }
inline int fileOf(int square) { return square & 0xf; }
inline bool offBoard(int square) { return (square & 0x88) != 0; }

inline int colorIndex(Color color) { return color == WHITE ? 0 : 1; }
inline Color swapColor(Color color) { return color == WHITE ? BLACK : WHITE; }

inline int pieceIndex(PieceSymbol type)
{
    switch (type) {
    case PAWN: return 0;
    case KNIGHT: return 1;
    case BISHOP: return 2;
    case ROOK: return 3;
    case QUEEN: return 4;
    case KING: return 5;
    }
    return 0;
}

inline int pieceMask(PieceSymbol type) { return 1 << pieceIndex(type); }

inline bool isPieceSymbol(char c)
{
    return c == PAWN || c == KNIGHT || c == BISHOP || c == ROOK || c == QUEEN || c == KING;
}

inline char toLower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
inline char toUpper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }
inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
inline bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }

inline std::string lowered(std::string_view text)
{
    std::string result(text);
    for (char& c : result) {
        c = toLower(c);
    }
    return result;
}

// Converts a 0x88 square to algebraic notation.
inline Square algebraic(int square)
{
    return {static_cast<char>('a' + fileOf(square)), static_cast<char>('8' - rankOf(square))};
}

// Converts algebraic notation to a 0x88 square, or EMPTY if invalid.
inline int ox88(std::string_view square)
{
    if (square.size() != 2 || square[0] < 'a' || square[0] > 'h' || square[1] < '1' || square[1] > '8') {
        return EMPTY;
    }
    return ('8' - square[1]) * 16 + (square[0] - 'a');
}

// Mirrors JavaScript's `text.split(/\s+/)`, including empty edge tokens.
inline std::vector<std::string> splitWhitespace(std::string_view text)
{
    std::vector<std::string> tokens;
    std::string current;
    std::size_t i = 0;
    while (i < text.size()) {
        if (isSpace(text[i])) {
            tokens.push_back(current);
            current.clear();
            while (i < text.size() && isSpace(text[i])) {
                ++i;
            }
        } else {
            current += text[i++];
        }
    }
    tokens.push_back(current);
    return tokens;
}

// Mirrors JavaScript's parseInt(text, 10): optional sign then leading digits.
inline std::optional<long long> parseIntJs(std::string_view text)
{
    std::size_t i = 0;
    while (i < text.size() && isSpace(text[i])) {
        ++i;
    }
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
        negative = text[i] == '-';
        ++i;
    }
    if (i >= text.size() || !isDigit(text[i])) {
        return std::nullopt;
    }
    long long value = 0;
    while (i < text.size() && isDigit(text[i])) {
        value = value * 10 + (text[i] - '0');
        ++i;
    }
    return negative ? -value : value;
}

inline std::string join(const std::vector<std::string>& parts, std::string_view separator)
{
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result += separator;
        }
        result += parts[i];
    }
    return result;
}

// A small insertion-ordered map, matching JavaScript object key ordering.
template <typename Value>
class OrderedMap {
public:
    const Value* find(std::string_view key) const
    {
        for (const auto& entry : entries_) {
            if (entry.first == key) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    Value* find(std::string_view key)
    {
        for (auto& entry : entries_) {
            if (entry.first == key) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    bool contains(std::string_view key) const { return find(key) != nullptr; }

    void set(std::string_view key, Value value)
    {
        if (Value* existing = find(key)) {
            *existing = std::move(value);
        } else {
            entries_.emplace_back(std::string(key), std::move(value));
        }
    }

    bool erase(std::string_view key)
    {
        const auto it = std::find_if(entries_.begin(), entries_.end(),
                                     [key](const auto& entry) { return entry.first == key; });
        if (it == entries_.end()) {
            return false;
        }
        entries_.erase(it);
        return true;
    }

    bool empty() const { return entries_.empty(); }
    void clear() { entries_.clear(); }

    auto begin() const { return entries_.begin(); }
    auto end() const { return entries_.end(); }

private:
    std::vector<std::pair<std::string, Value>> entries_;
};

using HeaderMap = OrderedMap<std::optional<std::string>>;

inline HeaderMap headerTemplate()
{
    HeaderMap header;
    for (const auto& [key, value] : SEVEN_TAG_ROSTER) {
        header.set(key, value);
    }
    // Placeholders that fix the order of supplemental tags as in the PGN spec.
    for (const char* key : {"WhiteTitle", "BlackTitle", "WhiteElo", "BlackElo", "WhiteUSCF", "BlackUSCF",
                            "WhiteNA", "BlackNA", "WhiteType", "BlackType", "EventDate", "EventSponsor",
                            "Section", "Stage", "Board", "Opening", "Variation", "SubVariation", "ECO", "NIC",
                            "Time", "UTCTime", "UTCDate", "TimeControl", "SetUp", "FEN", "Termination",
                            "Annotator", "Mode", "PlyCount"}) {
        header.set(key, std::nullopt);
    }
    return header;
}

inline std::optional<std::string> sevenTagDefault(std::string_view key)
{
    for (const auto& [tag, value] : SEVEN_TAG_ROSTER) {
        if (tag == key) {
            return value;
        }
    }
    return std::nullopt;
}

// xoroshiro128** with a 128-bit state, as used by chess.js for Zobrist keys.
class Xoroshiro128 {
public:
    Xoroshiro128(std::uint64_t high, std::uint64_t low)
        : s0_(low)
        , s1_(high)
    {
    }

    std::uint64_t operator()()
    {
        std::uint64_t s0 = s0_;
        std::uint64_t s1 = s1_;
        const std::uint64_t result = rotl(s0 * 5, 7) * 9;
        s1 ^= s0;
        s0 = rotl(s0, 24) ^ s1 ^ (s1 << 16);
        s1 = rotl(s1, 37);
        s0_ = s0;
        s1_ = s1;
        return result;
    }

private:
    static std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

    std::uint64_t s0_;
    std::uint64_t s1_;
};

struct ZobristKeys {
    std::uint64_t piece[2][6][128];
    std::uint64_t ep[8];
    std::uint64_t castling[16];
    std::uint64_t side;
};

inline const ZobristKeys& zobrist()
{
    static const ZobristKeys keys = [] {
        ZobristKeys k{};
        Xoroshiro128 rand(0xa187eb39cdcaed8fULL, 0x31c4b365b102e01eULL);
        for (auto& color : k.piece) {
            for (auto& type : color) {
                for (auto& square : type) {
                    square = rand();
                }
            }
        }
        for (auto& key : k.ep) {
            key = rand();
        }
        for (auto& key : k.castling) {
            key = rand();
        }
        k.side = rand();
        return k;
    }();
    return keys;
}

struct InternalMove {
    Color color = WHITE;
    int from = 0;
    int to = 0;
    PieceSymbol piece = PAWN;
    char captured = 0;  // PieceSymbol or 0
    char promotion = 0; // PieceSymbol or 0
    int flags = bits::NORMAL;
};

struct HistoryEntry {
    InternalMove move;
    std::array<int, 2> kings;
    Color turn;
    std::array<int, 2> castling;
    int epSquare;
    int fenEpSquare;
    int halfMoves;
    int moveNumber;
};

inline void addMove(std::vector<InternalMove>& moves, Color color, int from, int to, PieceSymbol piece,
                    char captured = 0, int flags = bits::NORMAL)
{
    const int r = rankOf(to);
    if (piece == PAWN && (r == RANK_1 || r == RANK_8)) {
        for (const PieceSymbol promotion : PROMOTIONS) {
            moves.push_back({color, from, to, piece, captured, static_cast<char>(promotion), flags | bits::PROMOTION});
        }
    } else {
        moves.push_back({color, from, to, piece, captured, 0, flags});
    }
}

// Used to uniquely identify ambiguous moves.
inline std::string getDisambiguator(const InternalMove& move, const std::vector<InternalMove>& moves)
{
    int ambiguities = 0;
    int sameRank = 0;
    int sameFile = 0;

    for (const InternalMove& other : moves) {
        // A move of the same piece type that ends on the same square needs a disambiguator.
        if (move.piece == other.piece && move.from != other.from && move.to == other.to) {
            ++ambiguities;
            if (rankOf(move.from) == rankOf(other.from)) {
                ++sameRank;
            }
            if (fileOf(move.from) == fileOf(other.from)) {
                ++sameFile;
            }
        }
    }

    if (ambiguities > 0) {
        const Square square = algebraic(move.from);
        if (sameRank > 0 && sameFile > 0) {
            return square;
        }
        if (sameFile > 0) {
            return square.substr(1, 1);
        }
        return square.substr(0, 1);
    }
    return {};
}

inline std::optional<char> inferPieceType(std::string_view san)
{
    if (san.empty()) {
        return std::nullopt;
    }
    const char first = san[0];
    if (first >= 'a' && first <= 'h') {
        static const std::regex fromTo("[a-h]\\d.*[a-h]\\d");
        if (std::regex_search(san.begin(), san.end(), fromTo)) {
            return std::nullopt;
        }
        return PAWN;
    }
    const char lower = toLower(first);
    if (lower == 'o') {
        return KING;
    }
    return lower;
}

// Parses all of the decorators out of a SAN string.
inline std::string strippedSan(std::string_view move)
{
    std::string result(move);
    const auto equals = result.find('=');
    if (equals != std::string::npos) {
        result.erase(equals, 1);
    }
    std::size_t end = result.size();
    while (end > 0 && (result[end - 1] == '?' || result[end - 1] == '!')) {
        --end;
    }
    if (end > 0 && (result[end - 1] == '+' || result[end - 1] == '#')) {
        --end;
    }
    result.resize(end);
    return result;
}

// --- PGN parsing (a hand-written equivalent of chess.js' pgn.peggy) ---------

struct PgnNode {
    std::string move;
    std::optional<std::string> suffixAnnotation;
    std::vector<NAG> nags;
    std::optional<std::string> comment;
};

struct ParsedPgn {
    OrderedMap<std::string> headers;
    std::optional<std::string> rootComment;
    std::vector<PgnNode> mainline;
    std::optional<std::string> result;
};

class PgnParser {
public:
    explicit PgnParser(std::string_view text)
        : text_(text)
    {
    }

    ParsedPgn parse()
    {
        ParsedPgn parsed;

        // tagPairSection = tagPair* _
        while (true) {
            const std::size_t start = pos_;
            std::pair<std::string, std::string> tag;
            if (!tagPair(tag)) {
                pos_ = start;
                break;
            }
            parsed.headers.set(tag.first, tag.second);
        }
        whitespace();

        // moveTextSection = line _ gameTerminationMarker? _
        line(&parsed.rootComment, &parsed.mainline);
        whitespace();

        std::optional<std::string> markerComment;
        const std::size_t markerStart = pos_;
        if (!terminationMarker(parsed.result, markerComment)) {
            pos_ = markerStart;
        }
        whitespace();

        if (pos_ != text_.size()) {
            throw Error("Invalid PGN: unexpected \"" + std::string(1, text_[pos_]) + "\" at offset " +
                        std::to_string(pos_));
        }

        if (markerComment) {
            if (parsed.mainline.empty()) {
                parsed.rootComment = markerComment;
            } else {
                parsed.mainline.back().comment = markerComment;
            }
        }
        return parsed;
    }

private:
    bool atEnd() const { return pos_ >= text_.size(); }
    char peek() const { return atEnd() ? '\0' : text_[pos_]; }

    bool literal(std::string_view expected)
    {
        if (text_.substr(pos_, expected.size()) == expected) {
            pos_ += expected.size();
            return true;
        }
        return false;
    }

    void whitespace()
    {
        while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')) {
            ++pos_;
        }
    }

    static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

    bool tagPair(std::pair<std::string, std::string>& tag)
    {
        whitespace();
        if (!literal("[")) {
            return false;
        }
        whitespace();
        const std::size_t nameStart = pos_;
        while (!atEnd() && isAlpha(peek())) {
            ++pos_;
        }
        if (pos_ == nameStart) {
            return false;
        }
        tag.first = std::string(text_.substr(nameStart, pos_ - nameStart));
        whitespace();
        if (!literal("\"")) {
            return false;
        }
        const std::size_t valueStart = pos_;
        while (!atEnd() && peek() != '"') {
            ++pos_;
        }
        tag.second = std::string(text_.substr(valueStart, pos_ - valueStart));
        if (!literal("\"")) {
            return false;
        }
        whitespace();
        return literal("]");
    }

    // line = comment? move*
    void line(std::optional<std::string>* rootComment, std::vector<PgnNode>* nodes)
    {
        const std::size_t commentStart = pos_;
        std::optional<std::string> leading = comment();
        if (!leading) {
            pos_ = commentStart;
        }
        if (rootComment) {
            *rootComment = leading;
        }

        while (true) {
            const std::size_t start = pos_;
            PgnNode node;
            if (!move(node)) {
                pos_ = start;
                break;
            }
            if (nodes) {
                nodes->push_back(std::move(node));
            }
        }
    }

    // move = _ moveNumber? _ san suffixAnnotation? nag* _ comment? variation*
    bool move(PgnNode& node)
    {
        whitespace();
        const std::size_t numberStart = pos_;
        if (!moveNumber()) {
            pos_ = numberStart;
        }
        whitespace();
        if (!san(node.move)) {
            return false;
        }

        const std::size_t suffixStart = pos_;
        while (pos_ - suffixStart < 2 && (peek() == '!' || peek() == '?')) {
            ++pos_;
        }
        if (pos_ > suffixStart) {
            node.suffixAnnotation = std::string(text_.substr(suffixStart, pos_ - suffixStart));
        }

        while (true) {
            const std::size_t start = pos_;
            whitespace();
            if (!literal("$") || !isDigit(peek())) {
                pos_ = start;
                break;
            }
            NAG nag = 0;
            while (isDigit(peek())) {
                if (nag > 99999) {
                    throw Error("Invalid PGN: NAG too large at offset " + std::to_string(pos_));
                }
                nag = nag * 10 + (peek() - '0');
                ++pos_;
            }
            node.nags.push_back(nag);
        }

        whitespace();
        const std::size_t commentStart = pos_;
        node.comment = comment();
        if (!node.comment) {
            pos_ = commentStart;
        }

        while (true) {
            const std::size_t start = pos_;
            if (!variation()) {
                pos_ = start;
                break;
            }
        }
        return true;
    }

    // moveNumber = [0-9]* '.' _ [.]*
    bool moveNumber()
    {
        while (isDigit(peek())) {
            ++pos_;
        }
        if (!literal(".")) {
            return false;
        }
        whitespace();
        while (peek() == '.') {
            ++pos_;
        }
        return true;
    }

    // san = ("O-O-O" / "O-O" / "0-0-0" / "0-0" / [a-zA-Z][a-zA-Z1-8-=]+) [+#]?
    bool san(std::string& out)
    {
        const std::size_t start = pos_;
        if (!(literal("O-O-O") || literal("O-O") || literal("0-0-0") || literal("0-0"))) {
            if (!isAlpha(peek())) {
                return false;
            }
            ++pos_;
            const std::size_t restStart = pos_;
            while (!atEnd()) {
                const char c = peek();
                if (isAlpha(c) || (c >= '1' && c <= '8') || c == '-' || c == '=') {
                    ++pos_;
                } else {
                    break;
                }
            }
            if (pos_ == restStart) {
                return false;
            }
        }
        if (peek() == '+' || peek() == '#') {
            ++pos_;
        }
        out = std::string(text_.substr(start, pos_ - start));
        return true;
    }

    std::optional<std::string> comment()
    {
        if (literal("{")) {
            const std::size_t start = pos_;
            while (!atEnd() && peek() != '}') {
                ++pos_;
            }
            if (!literal("}")) {
                return std::nullopt;
            }
            // Runs of line breaks become a single space.
            std::string result;
            const std::string_view raw = text_.substr(start, pos_ - 1 - start);
            for (std::size_t i = 0; i < raw.size(); ++i) {
                if (raw[i] == '\r' || raw[i] == '\n') {
                    while (i + 1 < raw.size() && (raw[i + 1] == '\r' || raw[i + 1] == '\n')) {
                        ++i;
                    }
                    result += ' ';
                } else {
                    result += raw[i];
                }
            }
            return result;
        }
        if (literal(";")) {
            const std::size_t start = pos_;
            while (!atEnd() && peek() != '\r' && peek() != '\n') {
                ++pos_;
            }
            std::string_view raw = text_.substr(start, pos_ - start);
            while (!raw.empty() && isSpace(raw.front())) {
                raw.remove_prefix(1);
            }
            while (!raw.empty() && isSpace(raw.back())) {
                raw.remove_suffix(1);
            }
            return std::string(raw);
        }
        return std::nullopt;
    }

    // variation = _ '(' line _ ')'   (variations are parsed but not kept)
    bool variation()
    {
        whitespace();
        if (!literal("(")) {
            return false;
        }
        line(nullptr, nullptr);
        whitespace();
        return literal(")");
    }

    // gameTerminationMarker = ('1-0' / '0-1' / '1/2-1/2' / '*') _ comment?
    bool terminationMarker(std::optional<std::string>& result, std::optional<std::string>& markerComment)
    {
        for (const char* marker : {"1-0", "0-1", "1/2-1/2", "*"}) {
            if (literal(marker)) {
                result = marker;
                whitespace();
                const std::size_t commentStart = pos_;
                markerComment = comment();
                if (!markerComment) {
                    pos_ = commentStart;
                }
                return true;
            }
        }
        return false;
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

} // namespace detail

// ---------------------------------------------------------------------------
// FEN validation
// ---------------------------------------------------------------------------

inline FenValidation validateFen(std::string_view fen)
{
    // 1st criterion: 6 space-separated fields?
    const std::vector<std::string> tokens = detail::splitWhitespace(fen);
    if (tokens.size() != 6) {
        return {false, "Invalid FEN: must contain six space-delimited fields"};
    }

    // 2nd criterion: move number field is an integer value > 0?
    const auto moveNumber = detail::parseIntJs(tokens[5]);
    if (!moveNumber || *moveNumber <= 0) {
        return {false, "Invalid FEN: move number must be a positive integer"};
    }

    // 3rd criterion: half move counter is an integer >= 0?
    const auto halfMoves = detail::parseIntJs(tokens[4]);
    if (!halfMoves || *halfMoves < 0) {
        return {false, "Invalid FEN: half move counter number must be a non-negative integer"};
    }

    // 4th criterion: 4th field is a valid e.p.-string?
    const std::string& ep = tokens[3];
    const bool validEp = ep == "-" || (ep.size() == 2 && ep[0] >= 'a' && ep[0] <= 'h' && (ep[1] == '3' || ep[1] == '6'));
    if (!validEp) {
        return {false, "Invalid FEN: en-passant square is invalid"};
    }

    // 5th criterion: 3rd field is a valid castle-string?
    if (tokens[2].find_first_not_of("kKqQ-") != std::string::npos) {
        return {false, "Invalid FEN: castling availability is invalid"};
    }

    // 6th criterion: 2nd field is "w" (white) or "b" (black)?
    if (tokens[1] != "w" && tokens[1] != "b") {
        return {false, "Invalid FEN: side-to-move is invalid"};
    }

    // 7th criterion: 1st field contains 8 rows?
    std::vector<std::string> rows;
    {
        std::string row;
        for (const char c : tokens[0]) {
            if (c == '/') {
                rows.push_back(row);
                row.clear();
            } else {
                row += c;
            }
        }
        rows.push_back(row);
    }
    if (rows.size() != 8) {
        return {false, "Invalid FEN: piece data does not contain 8 '/'-delimited rows"};
    }

    // 8th criterion: every row is valid?
    for (const std::string& row : rows) {
        // check for right sum of fields AND not two numbers in succession
        int sumFields = 0;
        bool previousWasNumber = false;
        for (const char c : row) {
            if (detail::isDigit(c)) {
                if (previousWasNumber) {
                    return {false, "Invalid FEN: piece data is invalid (consecutive number)"};
                }
                sumFields += c - '0';
                previousWasNumber = true;
            } else {
                if (std::string_view("prnbqkPRNBQK").find(c) == std::string_view::npos) {
                    return {false, "Invalid FEN: piece data is invalid (invalid piece)"};
                }
                sumFields += 1;
                previousWasNumber = false;
            }
        }
        if (sumFields != 8) {
            return {false, "Invalid FEN: piece data is invalid (too many squares in rank)"};
        }
    }

    // 9th criterion: is en-passant square legal?
    if (ep.size() > 1 && ((ep[1] == '3' && tokens[1] == "w") || (ep[1] == '6' && tokens[1] == "b"))) {
        return {false, "Invalid FEN: illegal en-passant square"};
    }

    // 10th criterion: does chess position contain exact two kings?
    for (const auto& [symbol, name] : {std::pair<char, const char*>{'K', "white"}, {'k', "black"}}) {
        const auto count = std::count(tokens[0].begin(), tokens[0].end(), symbol);
        if (count == 0) {
            return {false, std::string("Invalid FEN: missing ") + name + " king"};
        }
        if (count > 1) {
            return {false, std::string("Invalid FEN: too many ") + name + " kings"};
        }
    }

    // 11th criterion: are any pawns on the first or eighth rows?
    for (const char c : rows[0] + rows[7]) {
        if (detail::toUpper(c) == 'P') {
            return {false, "Invalid FEN: some pawns are on the edge rows"};
        }
    }

    return {true, {}};
}

// ---------------------------------------------------------------------------
// Chess
// ---------------------------------------------------------------------------

class Chess {
public:
    Chess()
        : Chess(DEFAULT_POSITION)
    {
    }

    explicit Chess(std::string_view fen, LoadOptions options = {})
    {
        load(fen, {options.skipValidation, false});
    }

    void clear(ClearOptions options = {})
    {
        board_.fill(std::nullopt);
        kings_ = {detail::EMPTY, detail::EMPTY};
        turn_ = WHITE;
        castling_ = {0, 0};
        epSquare_ = detail::EMPTY;
        fenEpSquare_ = detail::EMPTY;
        halfMoves_ = 0;
        moveNumber_ = 1;
        history_.clear();
        comments_.clear();
        if (!options.preserveHeaders) {
            header_ = detail::headerTemplate();
        }
        hash_ = computeHash();
        positionCount_.clear();

        // The board is empty, so these headers don't make sense in this state.
        // They'll be added again by load() or put().
        header_.set("SetUp", std::nullopt);
        header_.set("FEN", std::nullopt);
    }

    void load(std::string_view fenText, LoadOptions options = {})
    {
        std::string fen(fenText);
        std::vector<std::string> tokens = detail::splitWhitespace(fen);

        // Append commonly omitted fen tokens.
        if (tokens.size() >= 2 && tokens.size() < 6) {
            const std::vector<std::string> adjustments = {"-", "-", "0", "1"};
            for (std::size_t i = adjustments.size() - (6 - tokens.size()); i < adjustments.size(); ++i) {
                tokens.push_back(adjustments[i]);
            }
            fen = detail::join(tokens, " ");
        }

        tokens = detail::splitWhitespace(fen);

        if (!options.skipValidation) {
            const FenValidation validation = validateFen(fen);
            if (!validation.ok) {
                throw Error(validation.error);
            }
        }
        tokens.resize(std::max<std::size_t>(tokens.size(), 6));

        clear({options.preserveHeaders});

        int square = 0;
        for (const char c : tokens[0]) {
            if (c == '/') {
                square += 8;
            } else if (detail::isDigit(c)) {
                square += c - '0';
            } else {
                const Color color = c < 'a' ? WHITE : BLACK;
                const char type = detail::toLower(c);
                if (detail::isPieceSymbol(type) && square >= 0 && square < 128 && !detail::offBoard(square)) {
                    putInternal({color, static_cast<PieceSymbol>(type)}, square);
                }
                ++square;
            }
        }

        turn_ = tokens[1] == "b" ? BLACK : WHITE;

        const std::string& castling = tokens[2];
        if (castling.find('K') != std::string::npos) {
            castling_[0] |= detail::bits::KSIDE_CASTLE;
        }
        if (castling.find('Q') != std::string::npos) {
            castling_[0] |= detail::bits::QSIDE_CASTLE;
        }
        if (castling.find('k') != std::string::npos) {
            castling_[1] |= detail::bits::KSIDE_CASTLE;
        }
        if (castling.find('q') != std::string::npos) {
            castling_[1] |= detail::bits::QSIDE_CASTLE;
        }

        updateCastlingRights();

        epSquare_ = tokens[3] == "-" ? detail::EMPTY : detail::ox88(tokens[3]);
        fenEpSquare_ = epSquare_;
        halfMoves_ = static_cast<int>(detail::parseIntJs(tokens[4]).value_or(0));
        moveNumber_ = static_cast<int>(detail::parseIntJs(tokens[5]).value_or(1));

        hash_ = computeHash();
        updateSetup(fen);
        incPositionCount();
    }

    std::string fen(FenOptions options = {})
    {
        int empty = 0;
        std::string fen;

        for (int i = detail::SQ_A8; i <= detail::SQ_H1; ++i) {
            if (board_[i]) {
                if (empty > 0) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                const Piece& piece = *board_[i];
                fen += piece.color == WHITE ? detail::toUpper(piece.type) : detail::toLower(piece.type);
            } else {
                ++empty;
            }

            if (detail::offBoard(i + 1)) {
                if (empty > 0) {
                    fen += std::to_string(empty);
                }
                if (i != detail::SQ_H1) {
                    fen += '/';
                }
                empty = 0;
                i += 8;
            }
        }

        std::string castling;
        if (castling_[0] & detail::bits::KSIDE_CASTLE) {
            castling += 'K';
        }
        if (castling_[0] & detail::bits::QSIDE_CASTLE) {
            castling += 'Q';
        }
        if (castling_[1] & detail::bits::KSIDE_CASTLE) {
            castling += 'k';
        }
        if (castling_[1] & detail::bits::QSIDE_CASTLE) {
            castling += 'q';
        }
        if (castling.empty()) {
            castling = "-";
        }

        // Only print the ep square if en passant is a valid move (a pawn is
        // present and the ep capture is not pinned).
        std::string epSquare = "-";
        if (fenEpSquare_ != detail::EMPTY) {
            if (options.forceEnpassantSquare) {
                epSquare = detail::algebraic(fenEpSquare_);
            } else if (epSquare_ != detail::EMPTY) {
                const int bigPawnSquare = epSquare_ + (turn_ == WHITE ? 16 : -16);
                for (const int square : {bigPawnSquare + 1, bigPawnSquare - 1}) {
                    if (detail::offBoard(square)) {
                        continue;
                    }
                    const Color color = turn_;
                    if (board_[square] && board_[square]->color == color && board_[square]->type == PAWN) {
                        // Does the ep capture leave the king in check?
                        makeMove({color, square, epSquare_, PAWN, PAWN, 0, detail::bits::EP_CAPTURE});
                        const bool isLegal = !isKingAttacked(color);
                        undoMove();
                        if (isLegal) {
                            epSquare = detail::algebraic(epSquare_);
                            break;
                        }
                    }
                }
            }
        }

        return fen + ' ' + static_cast<char>(turn_) + ' ' + castling + ' ' + epSquare + ' ' +
               std::to_string(halfMoves_) + ' ' + std::to_string(moveNumber_);
    }

    void reset()
    {
        load(DEFAULT_POSITION);
        comments_.clear();
        suffixes_.clear();
        nags_.clear();
    }

    std::optional<Piece> get(std::string_view square) const
    {
        const int sq = detail::ox88(square);
        return sq == detail::EMPTY ? std::nullopt : board_[sq];
    }

    std::vector<Square> findPiece(const Piece& piece) const
    {
        std::vector<Square> squares;
        for (int i = detail::SQ_A8; i <= detail::SQ_H1; ++i) {
            if (detail::offBoard(i)) {
                i += 7;
                continue;
            }
            if (board_[i] && *board_[i] == piece) {
                squares.push_back(detail::algebraic(i));
            }
        }
        return squares;
    }

    bool put(const Piece& piece, std::string_view square)
    {
        const int sq = detail::ox88(square);
        if (sq == detail::EMPTY || !putInternal(piece, sq)) {
            return false;
        }
        updateCastlingRights();
        updateEnPassantSquare();
        updateSetup(fen());
        return true;
    }

    std::optional<Piece> remove(std::string_view square)
    {
        const int sq = detail::ox88(square);
        if (sq == detail::EMPTY) {
            return std::nullopt;
        }
        const std::optional<Piece> piece = board_[sq];
        clearSquare(sq);
        if (piece && piece->type == KING) {
            kings_[detail::colorIndex(piece->color)] = detail::EMPTY;
        }
        updateCastlingRights();
        updateEnPassantSquare();
        updateSetup(fen());
        return piece;
    }

    std::vector<Square> attackers(std::string_view square, std::optional<Color> attackedBy = std::nullopt) const
    {
        std::vector<Square> result;
        const int sq = detail::ox88(square);
        if (sq != detail::EMPTY) {
            attacked(attackedBy.value_or(turn_), sq, &result);
        }
        return result;
    }

    std::string hash() const
    {
        static constexpr char kDigits[] = "0123456789abcdef";
        if (hash_ == 0) {
            return "0";
        }
        std::string text;
        for (std::uint64_t value = hash_; value != 0; value >>= 4) {
            text.insert(text.begin(), kDigits[value & 0xf]);
        }
        return text;
    }

    bool isAttacked(std::string_view square, Color attackedBy) const
    {
        const int sq = detail::ox88(square);
        return sq != detail::EMPTY && attacked(attackedBy, sq);
    }

    bool isCheck() const { return isKingAttacked(turn_); }
    bool inCheck() const { return isCheck(); }

    bool isCheckmate() { return isCheck() && generateMoves().empty(); }
    bool isStalemate() { return !isCheck() && generateMoves().empty(); }

    bool isInsufficientMaterial() const
    {
        /*
         * k.b. vs k.b. (of opposite colors) with mate in 1:
         * 8/8/8/8/1b6/8/B1k5/K7 b - - 0 1
         *
         * k.b. vs k.n. with mate in 1:
         * 8/8/8/8/1n6/8/B7/K1k5 b - - 2 1
         */
        std::array<int, 6> pieces{};
        std::vector<int> bishops;
        int numPieces = 0;
        int squareColor = 0;

        for (int i = detail::SQ_A8; i <= detail::SQ_H1; ++i) {
            squareColor = (squareColor + 1) % 2;
            if (detail::offBoard(i)) {
                i += 7;
                continue;
            }
            if (board_[i]) {
                ++pieces[detail::pieceIndex(board_[i]->type)];
                if (board_[i]->type == BISHOP) {
                    bishops.push_back(squareColor);
                }
                ++numPieces;
            }
        }

        const int bishopCount = pieces[detail::pieceIndex(BISHOP)];
        const int knightCount = pieces[detail::pieceIndex(KNIGHT)];

        if (numPieces == 2) {
            return true; // k vs. k
        }
        if (numPieces == 3 && (bishopCount == 1 || knightCount == 1)) {
            return true; // k vs. kn .... or .... k vs. kb
        }
        if (numPieces == bishopCount + 2) {
            // kb vs. kb where any number of bishops are all on the same color
            int sum = 0;
            for (const int color : bishops) {
                sum += color;
            }
            if (sum == 0 || sum == static_cast<int>(bishops.size())) {
                return true;
            }
        }
        return false;
    }

    bool isThreefoldRepetition() const { return positionCount(hash_) >= 3; }

    bool isDrawByFiftyMoves() const { return halfMoves_ >= 100; } // 50 moves per side = 100 half moves

    bool isDraw() { return isDrawByFiftyMoves() || isStalemate() || isInsufficientMaterial() || isThreefoldRepetition(); }

    bool isGameOver() { return isCheckmate() || isDraw(); }

    bool isPromotion(std::string_view from, std::string_view to)
    {
        const int target = detail::ox88(to);
        for (const auto& move : generateMoves(true, PAWN, std::string(from))) {
            if (move.to == target && move.promotion) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> moves(const MovesOptions& options = {})
    {
        const auto moves = generateMoves(true, toFilter(options.piece), options.square);
        std::vector<std::string> result;
        result.reserve(moves.size());
        for (const auto& move : moves) {
            result.push_back(moveToSan(move, moves));
        }
        return result;
    }

    std::vector<Move> movesVerbose(const MovesOptions& options = {})
    {
        const auto moves = generateMoves(true, toFilter(options.piece), options.square);
        std::vector<Move> result;
        result.reserve(moves.size());
        for (const auto& move : moves) {
            result.push_back(createMove(move));
        }
        return result;
    }

    // Plays a move in Standard Algebraic Notation (case-sensitive). "--" is a null move.
    Move move(std::string_view san, MoveOptions options = {})
    {
        const auto internal = moveFromSan(san, options.strict);
        if (!internal) {
            throw Error("Invalid move: " + std::string(san));
        }
        return commitMove(*internal);
    }

    Move move(const MoveInput& input, MoveOptions = {})
    {
        std::optional<detail::InternalMove> internal;
        for (const auto& candidate : generateMoves()) {
            if (input.from == detail::algebraic(candidate.from) && input.to == detail::algebraic(candidate.to) &&
                (candidate.promotion == 0 || (input.promotion && *input.promotion == candidate.promotion))) {
                internal = candidate;
                break;
            }
        }
        if (!internal) {
            std::string description = "{\"from\":\"" + input.from + "\",\"to\":\"" + input.to + "\"";
            if (input.promotion) {
                description += ",\"promotion\":\"" + std::string(1, *input.promotion) + "\"";
            }
            throw Error("Invalid move: " + description + "}");
        }
        return commitMove(*internal);
    }

    // Plays a null move (passes the turn).
    Move move(std::nullptr_t, MoveOptions options = {}) { return move(detail::SAN_NULLMOVE, options); }

    std::optional<Move> undo()
    {
        const std::uint64_t hash = hash_;
        const auto move = undoMove();
        if (!move) {
            return std::nullopt;
        }
        Move pretty = createMove(*move);
        decPositionCount(hash);
        return pretty;
    }

    std::string pgn(const PgnOptions& options = {})
    {
        // Uses the specification from http://www.chessclub.com/help/PGN-spec
        const std::string& newline = options.newline;
        const auto maxWidth = static_cast<std::size_t>(std::max(options.maxWidth, 0));

        std::vector<std::string> result;
        bool headerExists = false;

        for (const auto& [key, value] : header_) {
            if (value && !value->empty()) {
                result.push_back("[" + key + " \"" + *value + "\"]" + newline);
            }
            headerExists = true;
        }

        if (headerExists && !history_.empty()) {
            result.push_back(newline);
        }

        const auto appendComment = [this](std::string moveString) {
            if (const std::string* comment = comments_.find(fen())) {
                const char* delimiter = moveString.empty() ? "" : " ";
                moveString += delimiter + std::string("{") + *comment + "}";
            }
            return moveString;
        };

        // Pop all of history onto reversedHistory.
        std::vector<detail::InternalMove> reversedHistory;
        while (!history_.empty()) {
            reversedHistory.push_back(*undoMove());
        }

        std::vector<std::string> moves;
        std::string moveString;

        // Special case of a commented starting position with no moves.
        if (reversedHistory.empty()) {
            moves.push_back(appendComment(""));
        }

        // Build the list of moves. A move string looks like: "3. e3 e6".
        while (!reversedHistory.empty()) {
            moveString = appendComment(moveString);
            const detail::InternalMove move = reversedHistory.back();
            reversedHistory.pop_back();

            if (history_.empty() && move.color == BLACK) {
                // The position started with black to move.
                const std::string prefix = std::to_string(moveNumber_) + ". ...";
                moveString = moveString.empty() ? prefix : moveString + " " + prefix;
            } else if (move.color == WHITE) {
                if (!moveString.empty()) {
                    moves.push_back(moveString);
                }
                moveString = std::to_string(moveNumber_) + ".";
            }

            moveString += " " + moveToSan(move, generateMoves());
            makeMove(move);
        }

        if (!moveString.empty()) {
            moves.push_back(appendComment(moveString));
        }

        // There always has to be a result according to the spec.
        const auto* resultTag = header_.find("Result");
        moves.push_back(resultTag && *resultTag && !(*resultTag)->empty() ? **resultTag : "*");

        if (maxWidth == 0) {
            return detail::join(result, "") + detail::join(moves, " ");
        }

        const auto strip = [&result] {
            if (!result.empty() && result.back() == " ") {
                result.pop_back();
                return true;
            }
            return false;
        };

        // NB: this does not preserve comment whitespace.
        const auto wrapComment = [&](std::size_t width, const std::string& move) {
            std::size_t start = 0;
            while (start <= move.size()) {
                std::size_t end = move.find(' ', start);
                if (end == std::string::npos) {
                    end = move.size();
                }
                const std::string token = move.substr(start, end - start);
                start = end + 1;
                if (token.empty()) {
                    continue;
                }
                if (width + token.size() > maxWidth) {
                    while (strip()) {
                        --width;
                    }
                    result.push_back(newline);
                    width = 0;
                }
                result.push_back(token);
                width += token.size();
                result.push_back(" ");
                ++width;
            }
            if (strip()) {
                --width;
            }
            return width;
        };

        // Wrap the PGN output at maxWidth.
        std::size_t currentWidth = 0;
        for (std::size_t i = 0; i < moves.size(); ++i) {
            if (currentWidth + moves[i].size() > maxWidth && moves[i].find('{') != std::string::npos) {
                currentWidth = wrapComment(currentWidth, moves[i]);
                continue;
            }
            if (currentWidth + moves[i].size() > maxWidth && i != 0) {
                // Don't end the line with whitespace.
                if (!result.empty() && result.back() == " ") {
                    result.pop_back();
                }
                result.push_back(newline);
                currentWidth = 0;
            } else if (i != 0) {
                result.push_back(" ");
                ++currentWidth;
            }
            result.push_back(moves[i]);
            currentWidth += moves[i].size();
        }

        return detail::join(result, "");
    }

    Headers setHeader(std::string_view key, std::string_view value)
    {
        header_.set(key, std::string(value));
        return getHeaders();
    }

    bool removeHeader(std::string_view key)
    {
        if (!header_.contains(key)) {
            return false;
        }
        header_.set(key, detail::sevenTagDefault(key));
        return true;
    }

    // Returns only non-null headers, in PGN order.
    Headers getHeaders() const
    {
        Headers headers;
        for (const auto& [key, value] : header_) {
            if (value) {
                headers.emplace_back(key, *value);
            }
        }
        return headers;
    }

    void loadPgn(std::string_view pgnText, const LoadPgnOptions& options = {})
    {
        std::string pgn(pgnText);
        if (options.newlineChar != "\r?\n") {
            pgn = std::regex_replace(pgn, std::regex(options.newlineChar), "\n");
        }

        const detail::ParsedPgn parsed = detail::PgnParser(pgn).parse();

        // Put the board in the starting position.
        reset();

        std::string fen;
        for (const auto& [key, value] : parsed.headers) {
            // The user may include a fen with the wrong tag case.
            if (detail::lowered(key) == "fen") {
                fen = value;
            }
            header_.set(key, value);
        }

        if (!options.strict) {
            // The permissive parser loads a FEN tag even without [SetUp "1"].
            if (!fen.empty()) {
                load(fen, {false, true});
            }
        } else {
            const std::string* setUp = parsed.headers.find("SetUp");
            if (setUp && *setUp == "1") {
                const std::string* fenTag = parsed.headers.find("FEN");
                if (!fenTag) {
                    throw Error("Invalid PGN: FEN tag must be supplied with SetUp tag");
                }
                load(*fenTag, {false, true});
            }
        }

        if (parsed.rootComment) {
            comments_.set(this->fen(), *parsed.rootComment);
        }

        for (const detail::PgnNode& node : parsed.mainline) {
            const auto move = moveFromSan(node.move, options.strict);
            if (!move) {
                throw Error("Invalid move in PGN: " + node.move);
            }
            makeMove(*move);
            incPositionCount();

            if (node.suffixAnnotation) {
                suffixes_.set(this->fen(), *node.suffixAnnotation);
            }
            if (!node.nags.empty()) {
                nags_.set(this->fen(), node.nags);
            }
            if (node.comment) {
                comments_.set(this->fen(), *node.comment);
            }
        }

        // Per section 8.2.6 of the PGN spec, the Result tag pair must match the
        // termination marker.
        if (parsed.result && !header_.empty()) {
            const auto* resultTag = header_.find("Result");
            if (!resultTag || *resultTag != parsed.result) {
                setHeader("Result", *parsed.result);
            }
        }
    }

    std::string ascii() const
    {
        std::string s = "   +------------------------+\n";
        for (int i = detail::SQ_A8; i <= detail::SQ_H1; ++i) {
            if (detail::fileOf(i) == 0) {
                s += std::string(" ") + "87654321"[detail::rankOf(i)] + " |";
            }
            if (board_[i]) {
                const Piece& piece = *board_[i];
                const char symbol = piece.color == WHITE ? detail::toUpper(piece.type) : detail::toLower(piece.type);
                s += std::string(" ") + symbol + " ";
            } else {
                s += " . ";
            }
            if (detail::offBoard(i + 1)) {
                s += "|\n";
                i += 8;
            }
        }
        s += "   +------------------------+\n";
        s += "     a  b  c  d  e  f  g  h";
        return s;
    }

    std::uint64_t perft(int depth)
    {
        const auto moves = generateMoves(false);
        std::uint64_t nodes = 0;
        const Color color = turn_;

        for (const auto& move : moves) {
            makeMove(move);
            if (!isKingAttacked(color)) {
                nodes += depth - 1 > 0 ? perft(depth - 1) : 1;
            }
            undoMove();
        }
        return nodes;
    }

    bool setTurn(Color color)
    {
        if (turn_ == color) {
            return false;
        }
        move(detail::SAN_NULLMOVE);
        return true;
    }

    Color turn() const { return turn_; }

    // Rows run from rank 8 to rank 1, files from a to h.
    std::array<std::array<std::optional<BoardSquare>, 8>, 8> board() const
    {
        std::array<std::array<std::optional<BoardSquare>, 8>, 8> output;
        for (int i = detail::SQ_A8; i <= detail::SQ_H1; ++i) {
            if (detail::offBoard(i)) {
                i += 7;
                continue;
            }
            if (board_[i]) {
                output[detail::rankOf(i)][detail::fileOf(i)] =
                    BoardSquare{detail::algebraic(i), board_[i]->type, board_[i]->color};
            }
        }
        return output;
    }

    // "light" or "dark", or nothing for an invalid square.
    std::optional<std::string> squareColor(std::string_view square) const
    {
        const int sq = detail::ox88(square);
        if (sq == detail::EMPTY) {
            return std::nullopt;
        }
        return (detail::rankOf(sq) + detail::fileOf(sq)) % 2 == 0 ? "light" : "dark";
    }

    std::vector<std::string> history()
    {
        std::vector<std::string> moveHistory;
        replayHistory([&](const detail::InternalMove& move) { moveHistory.push_back(moveToSan(move, generateMoves())); });
        return moveHistory;
    }

    std::vector<Move> historyVerbose()
    {
        std::vector<Move> moveHistory;
        replayHistory([&](const detail::InternalMove& move) { moveHistory.push_back(createMove(move)); });
        return moveHistory;
    }

    std::optional<std::string> getComment()
    {
        const std::string* comment = comments_.find(fen());
        return comment ? std::optional<std::string>(*comment) : std::nullopt;
    }

    void setComment(std::string_view text)
    {
        std::string comment(text);
        if (const auto open = comment.find('{'); open != std::string::npos) {
            comment[open] = '[';
        }
        if (const auto close = comment.find('}'); close != std::string::npos) {
            comment[close] = ']';
        }
        comments_.set(fen(), comment);
    }

    std::optional<std::string> removeComment()
    {
        const std::string key = fen();
        auto comment = getComment();
        comments_.erase(key);
        return comment;
    }

    std::vector<CommentEntry> getComments()
    {
        pruneComments();

        std::vector<std::string> allFenKeys;
        const auto addKey = [&allFenKeys](const std::string& fen) {
            if (std::find(allFenKeys.begin(), allFenKeys.end(), fen) == allFenKeys.end()) {
                allFenKeys.push_back(fen);
            }
        };
        for (const auto& entry : comments_) {
            addKey(entry.first);
        }
        for (const auto& entry : suffixes_) {
            addKey(entry.first);
        }
        for (const auto& entry : nags_) {
            addKey(entry.first);
        }

        std::vector<CommentEntry> result;
        for (const std::string& fen : allFenKeys) {
            CommentEntry entry{fen, std::nullopt, std::nullopt, {}};
            if (const std::string* comment = comments_.find(fen)) {
                entry.comment = *comment;
            }
            if (const std::string* suffix = suffixes_.find(fen)) {
                entry.suffixAnnotation = *suffix;
            }
            if (const std::vector<NAG>* nags = nags_.find(fen)) {
                entry.nags = *nags;
            }
            result.push_back(std::move(entry));
        }
        return result;
    }

    std::vector<FenComment> removeComments()
    {
        pruneComments();
        std::vector<FenComment> removed;
        for (const auto& [fen, comment] : comments_) {
            removed.push_back({fen, comment});
        }
        comments_.clear();
        return removed;
    }

    std::optional<std::string> getSuffixAnnotation(std::optional<std::string> fen = std::nullopt)
    {
        const std::string key = fen ? *fen : this->fen();
        const std::string* suffix = suffixes_.find(key);
        return suffix ? std::optional<std::string>(*suffix) : std::nullopt;
    }

    // Throws if the suffix isn't one of SUFFIX_LIST.
    void setSuffixAnnotation(std::string_view suffix, std::optional<std::string> fen = std::nullopt)
    {
        if (std::find(SUFFIX_LIST.begin(), SUFFIX_LIST.end(), suffix) == SUFFIX_LIST.end()) {
            throw Error("Invalid suffix: " + std::string(suffix));
        }
        suffixes_.set(fen ? *fen : this->fen(), std::string(suffix));
    }

    std::optional<std::string> removeSuffixAnnotation(std::optional<std::string> fen = std::nullopt)
    {
        const std::string key = fen ? *fen : this->fen();
        auto old = getSuffixAnnotation(key);
        suffixes_.erase(key);
        return old;
    }

    std::vector<NAG> getNags(std::optional<std::string> fen = std::nullopt)
    {
        const std::vector<NAG>* nags = nags_.find(fen ? *fen : this->fen());
        return nags ? *nags : std::vector<NAG>{};
    }

    void addNag(NAG nag, std::optional<std::string> fen = std::nullopt)
    {
        const std::string key = fen ? *fen : this->fen();
        std::vector<NAG> nags = getNags(key);
        if (std::find(nags.begin(), nags.end(), nag) == nags.end()) {
            nags.push_back(nag);
        }
        nags_.set(key, nags);
    }

    void setNags(std::vector<NAG> nags, std::optional<std::string> fen = std::nullopt)
    {
        nags_.set(fen ? *fen : this->fen(), std::move(nags));
    }

    std::vector<NAG> removeNags(std::optional<std::string> fen = std::nullopt)
    {
        const std::string key = fen ? *fen : this->fen();
        std::vector<NAG> old = getNags(key);
        nags_.erase(key);
        return old;
    }

    bool removeNag(NAG nag, std::optional<std::string> fen = std::nullopt)
    {
        const std::string key = fen ? *fen : this->fen();
        std::vector<NAG>* nags = nags_.find(key);
        if (!nags) {
            return false;
        }
        const auto it = std::find(nags->begin(), nags->end(), nag);
        if (it == nags->end()) {
            return false;
        }
        nags->erase(it);
        if (nags->empty()) {
            nags_.erase(key);
        }
        return true;
    }

    bool setCastlingRights(Color color, const CastlingRightsUpdate& rights)
    {
        int& castling = castling_[detail::colorIndex(color)];
        hash_ ^= castlingKey();
        if (rights.king) {
            castling = *rights.king ? castling | detail::bits::KSIDE_CASTLE : castling & ~detail::bits::KSIDE_CASTLE;
        }
        if (rights.queen) {
            castling = *rights.queen ? castling | detail::bits::QSIDE_CASTLE : castling & ~detail::bits::QSIDE_CASTLE;
        }
        hash_ ^= castlingKey();

        updateCastlingRights();
        const CastlingRights result = getCastlingRights(color);
        return (!rights.king || *rights.king == result.king) && (!rights.queen || *rights.queen == result.queen);
    }

    CastlingRights getCastlingRights(Color color) const
    {
        const int castling = castling_[detail::colorIndex(color)];
        return {(castling & detail::bits::KSIDE_CASTLE) != 0, (castling & detail::bits::QSIDE_CASTLE) != 0};
    }

    int moveNumber() const { return moveNumber_; }

private:
    using InternalMove = detail::InternalMove;

    static std::optional<char> toFilter(std::optional<PieceSymbol> piece)
    {
        return piece ? std::optional<char>(static_cast<char>(*piece)) : std::nullopt;
    }

    // --- Hashing --------------------------------------------------------------

    std::uint64_t pieceKey(int square) const
    {
        if (!board_[square]) {
            return 0;
        }
        const Piece& piece = *board_[square];
        return detail::zobrist().piece[detail::colorIndex(piece.color)][detail::pieceIndex(piece.type)][square];
    }

    std::uint64_t epKey() const { return epSquare_ == detail::EMPTY ? 0 : detail::zobrist().ep[epSquare_ & 7]; }

    std::uint64_t castlingKey() const { return detail::zobrist().castling[(castling_[0] >> 5) | (castling_[1] >> 3)]; }

    std::uint64_t computeHash() const
    {
        std::uint64_t hash = 0;
        for (int i = detail::SQ_A8; i <= detail::SQ_H1; ++i) {
            if (detail::offBoard(i)) {
                i += 7;
                continue;
            }
            hash ^= pieceKey(i);
        }
        hash ^= epKey();
        hash ^= castlingKey();
        if (turn_ == BLACK) {
            hash ^= detail::zobrist().side;
        }
        return hash;
    }

    int positionCount(std::uint64_t hash) const
    {
        const auto it = positionCount_.find(hash);
        return it == positionCount_.end() ? 0 : it->second;
    }

    void incPositionCount() { ++positionCount_[hash_]; }

    void decPositionCount(std::uint64_t hash)
    {
        const auto it = positionCount_.find(hash);
        if (it == positionCount_.end()) {
            return;
        }
        if (--it->second <= 0) {
            positionCount_.erase(it);
        }
    }

    // --- Board edits ------------------------------------------------------------

    // Called when the initial board setup is changed with put() or remove().
    // Only updates the SetUp and FEN headers while no moves have been made.
    void updateSetup(const std::string& fen)
    {
        if (!history_.empty()) {
            return;
        }
        if (fen != DEFAULT_POSITION) {
            header_.set("SetUp", std::string("1"));
            header_.set("FEN", fen);
        } else {
            header_.set("SetUp", std::nullopt);
            header_.set("FEN", std::nullopt);
        }
    }

    void setSquare(int square, const Piece& piece)
    {
        hash_ ^= pieceKey(square);
        board_[square] = piece;
        hash_ ^= pieceKey(square);
    }

    void clearSquare(int square)
    {
        hash_ ^= pieceKey(square);
        board_[square] = std::nullopt;
    }

    void movePiece(int from, int to)
    {
        hash_ ^= pieceKey(from);
        board_[to] = board_[from];
        board_[from] = std::nullopt;
        hash_ ^= pieceKey(to);
    }

    bool putInternal(const Piece& piece, int square)
    {
        if (!detail::isPieceSymbol(piece.type)) {
            return false;
        }

        // Don't let the user place more than one king.
        int& king = kings_[detail::colorIndex(piece.color)];
        if (piece.type == KING && !(king == detail::EMPTY || king == square)) {
            return false;
        }

        // If a king is being replaced, it no longer has a square.
        if (board_[square] && board_[square]->type == KING) {
            kings_[detail::colorIndex(board_[square]->color)] = detail::EMPTY;
        }

        setSquare(square, piece);

        if (piece.type == KING) {
            king = square;
        }
        return true;
    }

    bool isPieceAt(int square, Color color, PieceSymbol type) const
    {
        return board_[square] && board_[square]->color == color && board_[square]->type == type;
    }

    void updateCastlingRights()
    {
        hash_ ^= castlingKey();

        const bool whiteKingInPlace = isPieceAt(detail::SQ_E1, WHITE, KING);
        const bool blackKingInPlace = isPieceAt(detail::SQ_E8, BLACK, KING);

        if (!whiteKingInPlace || !isPieceAt(detail::SQ_A1, WHITE, ROOK)) {
            castling_[0] &= ~detail::bits::QSIDE_CASTLE;
        }
        if (!whiteKingInPlace || !isPieceAt(detail::SQ_H1, WHITE, ROOK)) {
            castling_[0] &= ~detail::bits::KSIDE_CASTLE;
        }
        if (!blackKingInPlace || !isPieceAt(detail::SQ_A8, BLACK, ROOK)) {
            castling_[1] &= ~detail::bits::QSIDE_CASTLE;
        }
        if (!blackKingInPlace || !isPieceAt(detail::SQ_H8, BLACK, ROOK)) {
            castling_[1] &= ~detail::bits::KSIDE_CASTLE;
        }

        hash_ ^= castlingKey();
    }

    void updateEnPassantSquare()
    {
        if (epSquare_ == detail::EMPTY) {
            return;
        }

        const int startSquare = epSquare_ + (turn_ == WHITE ? -16 : 16);
        const int currentSquare = epSquare_ + (turn_ == WHITE ? 16 : -16);

        const auto clearEp = [this] {
            hash_ ^= epKey();
            epSquare_ = detail::EMPTY;
        };

        if (board_[startSquare] || board_[epSquare_] || !isPieceAt(currentSquare, detail::swapColor(turn_), PAWN)) {
            clearEp();
            return;
        }

        const auto canCapture = [this](int square) { return !detail::offBoard(square) && isPieceAt(square, turn_, PAWN); };
        if (!canCapture(currentSquare + 1) && !canCapture(currentSquare - 1)) {
            clearEp();
        }
    }

    // --- Attacks ----------------------------------------------------------------

    bool attacked(Color color, int square, std::vector<Square>* attackers = nullptr) const
    {
        for (int i = detail::SQ_A8; i <= detail::SQ_H1; ++i) {
            if (detail::offBoard(i)) {
                i += 7;
                continue;
            }
            if (!board_[i] || board_[i]->color != color) {
                continue;
            }

            const Piece& piece = *board_[i];
            const int difference = i - square;
            if (difference == 0) {
                continue;
            }

            const int index = difference + 119;
            if (!(detail::ATTACKS[index] & detail::pieceMask(piece.type))) {
                continue;
            }

            bool attacks = false;
            if (piece.type == PAWN) {
                attacks = (difference > 0 && piece.color == WHITE) || (difference <= 0 && piece.color == BLACK);
            } else if (piece.type == KNIGHT || piece.type == KING) {
                attacks = true;
            } else {
                const int offset = detail::RAYS[index];
                int j = i + offset;
                bool blocked = false;
                while (j != square) {
                    if (board_[j]) {
                        blocked = true;
                        break;
                    }
                    j += offset;
                }
                attacks = !blocked;
            }

            if (attacks) {
                if (!attackers) {
                    return true;
                }
                attackers->push_back(detail::algebraic(i));
            }
        }
        return false;
    }

    bool isKingAttacked(Color color) const
    {
        const int square = kings_[detail::colorIndex(color)];
        return square != detail::EMPTY && attacked(detail::swapColor(color), square);
    }

    // --- Move generation ----------------------------------------------------------

    std::vector<InternalMove> generateMoves(bool legal = true, std::optional<char> piece = std::nullopt,
                                            const std::optional<std::string>& square = std::nullopt)
    {
        const std::optional<char> forPiece = piece ? std::optional<char>(detail::toLower(*piece)) : std::nullopt;

        std::vector<InternalMove> moves;
        const Color us = turn_;
        const Color them = detail::swapColor(us);
        const int usIndex = detail::colorIndex(us);

        int firstSquare = detail::SQ_A8;
        int lastSquare = detail::SQ_H1;
        bool singleSquare = false;

        // Are we generating moves for a single square?
        if (square) {
            const int sq = detail::ox88(detail::lowered(*square));
            if (sq == detail::EMPTY) {
                return {};
            }
            firstSquare = lastSquare = sq;
            singleSquare = true;
        }

        for (int from = firstSquare; from <= lastSquare; ++from) {
            if (detail::offBoard(from)) {
                from += 7;
                continue;
            }
            if (!board_[from] || board_[from]->color == them) {
                continue;
            }
            const PieceSymbol type = board_[from]->type;
            if (forPiece && *forPiece != type) {
                continue;
            }

            if (type == PAWN) {
                const auto& offsets = us == WHITE ? detail::PAWN_OFFSETS_WHITE : detail::PAWN_OFFSETS_BLACK;

                // Single square, non-capturing.
                int to = from + offsets[0];
                if (!board_[to]) {
                    detail::addMove(moves, us, from, to, PAWN);

                    // Double square.
                    to = from + offsets[1];
                    const int secondRank = us == WHITE ? detail::RANK_2 : detail::RANK_7;
                    if (secondRank == detail::rankOf(from) && !board_[to]) {
                        detail::addMove(moves, us, from, to, PAWN, 0, detail::bits::BIG_PAWN);
                    }
                }

                // Pawn captures.
                for (int j = 2; j < 4; ++j) {
                    to = from + offsets[j];
                    if (detail::offBoard(to)) {
                        continue;
                    }
                    if (board_[to] && board_[to]->color == them) {
                        detail::addMove(moves, us, from, to, PAWN, board_[to]->type, detail::bits::CAPTURE);
                    } else if (to == epSquare_) {
                        detail::addMove(moves, us, from, to, PAWN, PAWN, detail::bits::EP_CAPTURE);
                    }
                }
            } else {
                for (const int offset : pieceOffsets(type)) {
                    int to = from;
                    while (true) {
                        to += offset;
                        if (detail::offBoard(to)) {
                            break;
                        }
                        if (!board_[to]) {
                            detail::addMove(moves, us, from, to, type);
                        } else {
                            if (board_[to]->color == us) {
                                break;
                            }
                            detail::addMove(moves, us, from, to, type, board_[to]->type, detail::bits::CAPTURE);
                            break;
                        }
                        if (type == KNIGHT || type == KING) {
                            break;
                        }
                    }
                }
            }
        }

        // Check for castling if we're generating all moves, or doing single
        // square move generation on the king's square.
        if ((!forPiece || *forPiece == KING) && (!singleSquare || lastSquare == kings_[usIndex])) {
            const int kingSquare = kings_[usIndex];

            if (castling_[usIndex] & detail::bits::KSIDE_CASTLE) {
                const int castlingTo = kingSquare + 2;
                if (!board_[kingSquare + 1] && !board_[castlingTo] && !attacked(them, kingSquare) &&
                    !attacked(them, kingSquare + 1) && !attacked(them, castlingTo)) {
                    detail::addMove(moves, us, kingSquare, castlingTo, KING, 0, detail::bits::KSIDE_CASTLE);
                }
            }

            if (castling_[usIndex] & detail::bits::QSIDE_CASTLE) {
                const int castlingTo = kingSquare - 2;
                if (!board_[kingSquare - 1] && !board_[kingSquare - 2] && !board_[kingSquare - 3] &&
                    !attacked(them, kingSquare) && !attacked(them, kingSquare - 1) && !attacked(them, castlingTo)) {
                    detail::addMove(moves, us, kingSquare, castlingTo, KING, 0, detail::bits::QSIDE_CASTLE);
                }
            }
        }

        // Pseudo-legal moves include moves that allow the king to be captured.
        if (!legal || kings_[usIndex] == detail::EMPTY) {
            return moves;
        }

        std::vector<InternalMove> legalMoves;
        legalMoves.reserve(moves.size());
        for (const auto& move : moves) {
            makeMove(move);
            if (!isKingAttacked(us)) {
                legalMoves.push_back(move);
            }
            undoMove();
        }
        return legalMoves;
    }

    struct OffsetRange {
        const int* first = nullptr;
        const int* last = nullptr;
        const int* begin() const { return first; }
        const int* end() const { return last; }
    };

    template <std::size_t N>
    static OffsetRange range(const std::array<int, N>& offsets)
    {
        return {offsets.data(), offsets.data() + N};
    }

    static OffsetRange pieceOffsets(PieceSymbol type)
    {
        switch (type) {
        case KNIGHT: return range(detail::KNIGHT_OFFSETS);
        case BISHOP: return range(detail::BISHOP_OFFSETS);
        case ROOK: return range(detail::ROOK_OFFSETS);
        case QUEEN:
        case KING: return range(detail::KING_OFFSETS);
        default: return {};
        }
    }

    // --- Making and unmaking moves -------------------------------------------------

    Move commitMove(const InternalMove& internal)
    {
        // Disallow null moves when in check.
        if (isCheck() && (internal.flags & detail::bits::NULL_MOVE)) {
            throw Error("Null move not allowed when in check");
        }

        // SAN must be generated before the move is made.
        Move pretty = createMove(internal);
        makeMove(internal);
        incPositionCount();
        return pretty;
    }

    Move createMove(const InternalMove& internal)
    {
        Move move;
        move.san = moveToSan(internal, generateMoves());
        move.before = fen();
        makeMove(internal);
        move.after = fen();
        undoMove();

        move.color = internal.color;
        move.piece = internal.piece;
        move.from = detail::algebraic(internal.from);
        move.to = detail::algebraic(internal.to);
        move.lan = move.from + move.to;

        for (const auto& [bit, flag] : detail::FLAG_CHARS) {
            if (internal.flags & bit) {
                move.flags += flag;
            }
        }
        if (internal.captured) {
            move.captured = static_cast<PieceSymbol>(internal.captured);
        }
        if (internal.promotion) {
            move.promotion = static_cast<PieceSymbol>(internal.promotion);
            move.lan += internal.promotion;
        }
        return move;
    }

    void pushHistory(const InternalMove& move)
    {
        history_.push_back({move, kings_, turn_, castling_, epSquare_, fenEpSquare_, halfMoves_, moveNumber_});
    }

    void makeMove(const InternalMove& move)
    {
        const Color us = turn_;
        const Color them = detail::swapColor(us);
        const int usIndex = detail::colorIndex(us);
        const int themIndex = detail::colorIndex(them);
        pushHistory(move);

        if (move.flags & detail::bits::NULL_MOVE) {
            if (us == BLACK) {
                ++moveNumber_;
            }
            ++halfMoves_;
            hash_ ^= epKey();
            epSquare_ = detail::EMPTY;
            turn_ = them;
            hash_ ^= detail::zobrist().side;
            return;
        }

        hash_ ^= epKey();
        hash_ ^= castlingKey();

        if (move.captured) {
            hash_ ^= pieceKey(move.to);
        }

        movePiece(move.from, move.to);

        // If ep capture, remove the captured pawn.
        if (move.flags & detail::bits::EP_CAPTURE) {
            clearSquare(turn_ == BLACK ? move.to - 16 : move.to + 16);
        }

        // If pawn promotion, replace with new piece.
        if (move.promotion) {
            clearSquare(move.to);
            setSquare(move.to, {us, static_cast<PieceSymbol>(move.promotion)});
        }

        // If we moved the king.
        if (board_[move.to]->type == KING) {
            kings_[usIndex] = move.to;

            // If we castled, move the rook next to the king.
            if (move.flags & detail::bits::KSIDE_CASTLE) {
                movePiece(move.to + 1, move.to - 1);
            } else if (move.flags & detail::bits::QSIDE_CASTLE) {
                movePiece(move.to - 2, move.to + 1);
            }

            castling_[usIndex] = 0;
        }

        const auto rookSquares = [](Color color) {
            return color == WHITE ? std::array<std::pair<int, int>, 2>{{{detail::SQ_A1, detail::bits::QSIDE_CASTLE},
                                                                        {detail::SQ_H1, detail::bits::KSIDE_CASTLE}}}
                                  : std::array<std::pair<int, int>, 2>{{{detail::SQ_A8, detail::bits::QSIDE_CASTLE},
                                                                        {detail::SQ_H8, detail::bits::KSIDE_CASTLE}}};
        };

        // Turn off castling if we move a rook.
        if (castling_[usIndex]) {
            for (const auto& [square, flag] : rookSquares(us)) {
                if (move.from == square && (castling_[usIndex] & flag)) {
                    castling_[usIndex] ^= flag;
                    break;
                }
            }
        }

        // Turn off castling if we capture a rook.
        if (castling_[themIndex]) {
            for (const auto& [square, flag] : rookSquares(them)) {
                if (move.to == square && (castling_[themIndex] & flag)) {
                    castling_[themIndex] ^= flag;
                    break;
                }
            }
        }

        hash_ ^= castlingKey();

        // If big pawn move, update the en passant square.
        if (move.flags & detail::bits::BIG_PAWN) {
            const int epSquare = us == BLACK ? move.to - 16 : move.to + 16;
            fenEpSquare_ = epSquare;

            const bool capturable = (!detail::offBoard(move.to - 1) && isPieceAt(move.to - 1, them, PAWN)) ||
                                    (!detail::offBoard(move.to + 1) && isPieceAt(move.to + 1, them, PAWN));
            if (capturable) {
                epSquare_ = epSquare;
                hash_ ^= epKey();
            } else {
                epSquare_ = detail::EMPTY;
            }
        } else {
            epSquare_ = detail::EMPTY;
            fenEpSquare_ = detail::EMPTY;
        }

        // Reset the 50 move counter if a pawn is moved or a piece is captured.
        if (move.piece == PAWN || (move.flags & (detail::bits::CAPTURE | detail::bits::EP_CAPTURE))) {
            halfMoves_ = 0;
        } else {
            ++halfMoves_;
        }

        if (us == BLACK) {
            ++moveNumber_;
        }

        turn_ = them;
        hash_ ^= detail::zobrist().side;
    }

    std::optional<InternalMove> undoMove()
    {
        if (history_.empty()) {
            return std::nullopt;
        }
        const detail::HistoryEntry old = history_.back();
        history_.pop_back();

        hash_ ^= epKey();
        hash_ ^= castlingKey();

        const InternalMove& move = old.move;
        kings_ = old.kings;
        turn_ = old.turn;
        castling_ = old.castling;
        epSquare_ = old.epSquare;
        fenEpSquare_ = old.fenEpSquare;
        halfMoves_ = old.halfMoves;
        moveNumber_ = old.moveNumber;

        hash_ ^= epKey();
        hash_ ^= castlingKey();
        hash_ ^= detail::zobrist().side;

        const Color us = turn_;
        const Color them = detail::swapColor(us);

        if (move.flags & detail::bits::NULL_MOVE) {
            return move;
        }

        movePiece(move.to, move.from);

        // Undo any promotion.
        clearSquare(move.from);
        setSquare(move.from, {us, move.piece});

        if (move.captured) {
            if (move.flags & detail::bits::EP_CAPTURE) {
                setSquare(us == BLACK ? move.to - 16 : move.to + 16, {them, PAWN});
            } else {
                setSquare(move.to, {them, static_cast<PieceSymbol>(move.captured)});
            }
        }

        if (move.flags & detail::bits::KSIDE_CASTLE) {
            movePiece(move.to - 1, move.to + 1);
        } else if (move.flags & detail::bits::QSIDE_CASTLE) {
            movePiece(move.to + 1, move.to - 2);
        }

        return move;
    }

    // Undoes every move, then replays them, calling visit() before each one.
    template <typename Visitor>
    void replayHistory(Visitor visit)
    {
        std::vector<InternalMove> reversedHistory;
        while (!history_.empty()) {
            reversedHistory.push_back(*undoMove());
        }
        while (!reversedHistory.empty()) {
            const InternalMove move = reversedHistory.back();
            reversedHistory.pop_back();
            visit(move);
            makeMove(move);
        }
    }

    void pruneComments()
    {
        detail::OrderedMap<std::string> currentComments;
        const auto copyComment = [&](const std::string& fen) {
            if (const std::string* comment = comments_.find(fen)) {
                currentComments.set(fen, *comment);
            }
        };

        std::vector<InternalMove> reversedHistory;
        while (!history_.empty()) {
            reversedHistory.push_back(*undoMove());
        }
        copyComment(fen());
        while (!reversedHistory.empty()) {
            makeMove(reversedHistory.back());
            reversedHistory.pop_back();
            copyComment(fen());
        }
        comments_ = std::move(currentComments);
    }

    // --- SAN ------------------------------------------------------------------------

    std::string moveToSan(const InternalMove& move, const std::vector<InternalMove>& moves)
    {
        std::string output;

        if (move.flags & detail::bits::KSIDE_CASTLE) {
            output = "O-O";
        } else if (move.flags & detail::bits::QSIDE_CASTLE) {
            output = "O-O-O";
        } else if (move.flags & detail::bits::NULL_MOVE) {
            return detail::SAN_NULLMOVE;
        } else {
            if (move.piece != PAWN) {
                output += detail::toUpper(move.piece) + detail::getDisambiguator(move, moves);
            }
            if (move.flags & (detail::bits::CAPTURE | detail::bits::EP_CAPTURE)) {
                if (move.piece == PAWN) {
                    output += detail::algebraic(move.from)[0];
                }
                output += 'x';
            }
            output += detail::algebraic(move.to);
            if (move.promotion) {
                output += '=';
                output += detail::toUpper(move.promotion);
            }
        }

        makeMove(move);
        if (isCheck()) {
            output += isCheckmate() ? '#' : '+';
        }
        undoMove();

        return output;
    }

    /*
     * Converts a SAN string to a move. The strict parser only accepts exact
     * SAN; the permissive parser also accepts notations like Pe2-e4, Rc1c4,
     * Qf3xf7, f7f8q, b1c3 and overly disambiguated moves such as Nge7.
     */
    std::optional<InternalMove> moveFromSan(std::string_view moveText, bool strict)
    {
        // Strip off any move decorations: e.g. Nf3+?! becomes Nf3.
        std::string cleanMove = detail::strippedSan(moveText);

        if (!strict) {
            if (cleanMove == "0-0") {
                cleanMove = "O-O";
            } else if (cleanMove == "0-0-0") {
                cleanMove = "O-O-O";
            }
        }

        if (cleanMove == detail::SAN_NULLMOVE) {
            return InternalMove{turn_, 0, 0, KING, 0, 0, detail::bits::NULL_MOVE};
        }

        std::optional<char> pieceType = detail::inferPieceType(cleanMove);
        std::vector<InternalMove> moves = generateMoves(true, pieceType);

        // Strict parser.
        for (const auto& move : moves) {
            if (cleanMove == detail::strippedSan(moveToSan(move, moves))) {
                return move;
            }
        }

        if (strict) {
            return std::nullopt;
        }

        // Permissive parser: grab the piece, from/to squares and promotion.
        static const std::regex fullMove("([pnbrqkPNBRQK])?([a-h][1-8])x?-?([a-h][1-8])([qrbnQRBN])?");
        static const std::regex partialMove("([pnbrqkPNBRQK])?([a-h]?[1-8]?)x?-?([a-h][1-8])([qrbnQRBN])?");

        std::optional<char> piece;
        std::string from;
        std::string to;
        std::optional<char> promotion;
        bool overlyDisambiguated = false;

        std::smatch matches;
        if (std::regex_search(cleanMove, matches, fullMove) || std::regex_search(cleanMove, matches, partialMove)) {
            if (matches[1].matched) {
                piece = matches[1].str()[0];
            }
            from = matches[2].str();
            to = matches[3].str();
            if (matches[4].matched) {
                promotion = matches[4].str()[0];
            }
            // A single rank or file is an unnecessary disambiguator (e.g. Nge7).
            overlyDisambiguated = from.size() == 1;
        }

        pieceType = detail::inferPieceType(cleanMove);
        moves = generateMoves(true, piece ? piece : pieceType);

        if (to.empty()) {
            return std::nullopt;
        }

        for (const auto& move : moves) {
            const bool pieceMatches = !piece || detail::toLower(*piece) == move.piece;
            const bool promotionMatches = !promotion || detail::toLower(*promotion) == move.promotion;

            if (from.empty()) {
                // Without a from square, it could be just the 'x' missing from a capture.
                std::string san = detail::strippedSan(moveToSan(move, moves));
                if (const auto x = san.find('x'); x != std::string::npos) {
                    san.erase(x, 1);
                }
                if (cleanMove == san) {
                    return move;
                }
            } else if (pieceMatches && detail::ox88(from) == move.from && detail::ox88(to) == move.to &&
                       promotionMatches) {
                return move;
            } else if (overlyDisambiguated) {
                const Square square = detail::algebraic(move.from);
                if (pieceMatches && detail::ox88(to) == move.to && (from[0] == square[0] || from[0] == square[1]) &&
                    promotionMatches) {
                    return move;
                }
            }
        }

        return std::nullopt;
    }

    std::array<std::optional<Piece>, 128> board_{};
    Color turn_ = WHITE;
    detail::HeaderMap header_;
    std::array<int, 2> kings_{detail::EMPTY, detail::EMPTY};
    int epSquare_ = detail::EMPTY;
    int fenEpSquare_ = detail::EMPTY;
    int halfMoves_ = 0;
    int moveNumber_ = 0;
    std::vector<detail::HistoryEntry> history_;
    detail::OrderedMap<std::string> comments_;
    detail::OrderedMap<std::string> suffixes_;
    detail::OrderedMap<std::vector<NAG>> nags_;
    std::array<int, 2> castling_{0, 0};
    std::uint64_t hash_ = 0;

    // Tracks how many times each position has been seen, for repetition checks.
    std::unordered_map<std::uint64_t, int> positionCount_;
};

} // namespace chessjs
