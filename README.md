# chessjs-cpp

A header-only C++17 port of [chess.js](https://github.com/jhlywa/chess.js): move generation and validation, SAN, FEN,
PGN reading and writing, check, checkmate and draw detection, undo, comments and NAGs, with the same API as chess.js.
One file, `chessjs.hpp`, and nothing else to build or link.

I wrote it for a personal project (a chess game in C++) and wanted chess.js's API there. It's free to use if anybody
wants it. It is an unofficial port and isn't affiliated with or endorsed by the chess.js project.

## Example

```cpp
#include <chessjs.hpp>
#include <iostream>

int main()
{
    chessjs::Chess chess;

    chess.move("e4");                              // SAN
    chess.move(chessjs::MoveInput{"e7", "e5"});    // from / to squares
    chess.move("g1f3");                            // long algebraic, accepted by the permissive parser

    for (const std::string& san : chess.moves()) { // every legal move
        std::cout << san << ' ';
    }
    std::cout << '\n';

    for (const chessjs::Move& move : chess.movesVerbose({"f8"})) {
        std::cout << move.san << ": " << move.from << " -> " << move.to << '\n';
    }

    chess.undo();
    std::cout << chess.fen() << '\n' << chess.ascii() << '\n';

    chess.loadPgn("1. f3 e5 2. g4 Qh4# 0-1");
    if (chess.isCheckmate()) {
        std::cout << chess.pgn() << '\n';
    }
}
```

[examples/example.cpp](examples/example.cpp) is a longer tour (errors, headers, comments).

## Adding it to a project

It's a single header, so copying `include/chessjs.hpp` into your project works. With CMake:

```cmake
include(FetchContent)
FetchContent_Declare(
    chessjs
    GIT_REPOSITORY https://github.com/RatPrez/chessjs-cpp.git
    GIT_TAG        v1.0.0        # a release tag (or a commit hash); main follows the latest
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(chessjs)

target_link_libraries(my_app PRIVATE chessjs::chessjs)
```

or, with a copy of the repository next to your project, `add_subdirectory(chessjs-cpp)` and the same
`target_link_libraries`. Linking `chessjs::chessjs` adds the include directory and C++17 (or newer) to your target.

It also installs as a CMake package:

```
cmake -S . -B build
cmake --install build --prefix <prefix>
```

```cmake
find_package(chessjs 1.0 REQUIRED)
target_link_libraries(my_app PRIVATE chessjs::chessjs)
```

Building the repository on its own also builds the example (`CHESSJS_BUILD_EXAMPLES`).

## Differences from chess.js

The [chess.js documentation](https://github.com/jhlywa/chess.js#api) applies, with these changes for C++:

| chess.js | chessjs.hpp |
|---|---|
| `moves({ verbose: true })`, `history({ verbose: true })` | `movesVerbose()`, `historyVerbose()` |
| option objects, e.g. `moves({ square: 'e2' })` | option structs, e.g. `moves({"e2"})` or `MovesOptions{...}` |
| `move({ from, to, promotion })` | `move(MoveInput{from, to, promotion})` |
| `move(null)` (null move) | `move(nullptr)` or `move("--")` |
| methods that throw | throw `chessjs::Error` |
| `undefined` / `null` results | `std::optional` |
| headers as an object | `getHeaders()` returns key/value pairs in PGN order |

The deprecated `header()`, `deleteComment()` and `deleteComments()` aren't ported. A few behaviours differ on purpose,
each fixing a chess.js bug so that `hash()` and threefold repetition stay consistent with the position:

- `put()` and `remove()` only clear the en passant square when it stops being capturable.
- Null moves update the position hash, so undoing one restores it.
- `setCastlingRights()` keeps the position hash in sync with the rights.

A PGN NAG above 999999 throws `chessjs::Error` (a NAG is an `int`; the PGN standard defines 0 to 255).

It's ported from chess.js's `main` branch at commit `d43e668`, which is newer than the 1.4.0 npm release (it adds NAGs
and `Move::isCheck()`).

## Speed

It keeps chess.js's design (a 0x88 board and string-based move objects), so it's built for convenience rather than
speed. For engines or processing millions of games, [chess-library](https://github.com/Disservin/chess-library) is a
better fit.

## AI usage

chessjs-cpp is developed with assistance from Claude (Anthropic). The code is written with Claude's assistance under the
author's direction, and the documentation is written by Claude. The behaviour comes from chess.js's source (credited
below), and the port is checked by playing the same random games in chess.js and in chessjs.hpp and comparing every
result: moves, FEN, hashes, SAN, game state, undo, PGN in and out, comments and perft.

## Credits and licence

BSD-2-Clause (see [LICENSE](LICENSE)), the same as chess.js. chess.js is by Jeff Hlywa and contributors.
