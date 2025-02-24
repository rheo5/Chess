#include "game.hpp"
#include "move.hpp"
#include "pieces.hpp"
#include <cmath>
#include <map>
#include <memory>
#include <utility>

ChessGame::ChessGame() {}

ChessGame::ChessGame(const ChessGame &other)
    : board(std::make_shared<Board>(*other.board)),
      whitePlayer(other.whitePlayer), blackPlayer(other.blackPlayer),
      turn(other.turn), gameState(other.gameState), moveList(other.moveList) {}

bool withinBoard(int row, int col) {
    return row >= 0 && col >= 0 && row < 8 && col < 8;
}

void ChessGame::computeStalemate() {
    if (gameState != (turn == White ? CheckForBlack : CheckForWhite) &&
        gameState != (turn == White ? CheckmateForBlack : CheckmateForWhite) &&
        generateLegalMoves().size() == 0) {
        gameState = Stalemate;
    }
}

void ChessGame::computeState(PieceColor color) {
    if (isKingInCheckInternal(color, *board)) {
        gameState = color == White ? CheckForBlack : CheckForWhite;

        std::vector<Move> opponentMoves = generateLegalMovesInternal(
            color, *board);

        // if none of these moves result in being out of check ->
        // checkmate
        bool outOfCheck = false;
        for (const auto &m : opponentMoves) {
            Board preliminaryOpponentBoard = Board(*board);
            preliminaryOpponentBoard.move(m);

            if (!isKingInCheckInternal(color,
                               preliminaryOpponentBoard)) {
                outOfCheck = true;
                break;
            }
        }

        if (!outOfCheck) {
            gameState = color == White ? CheckmateForBlack : CheckmateForWhite;
        }
    }
}

std::pair<int, int> ChessGame::findKing(PieceColor color,
                                        const Board &board) const {
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            std::shared_ptr<Piece> piece = board.getPieceAt(i, j);
            if (std::dynamic_pointer_cast<King>(piece) &&
                piece->getColor() == color) {
                return std::make_pair(i, j);
            }
        }
    }

    return std::make_pair(-1, -1);
}

bool ChessGame::isKingInCheckInternal(PieceColor kingColor, const Board &board) const {
    std::vector<Move> opponentMoves =
        generateLegalMovesInternal(kingColor == White ? Black : White, board);

    std::pair<int, int> king = findKing(kingColor, board);
    for (const auto &m : opponentMoves) {
        if (isCheckInternal(m, king)) {
            return true;
        }
    }

    return false;
}

bool ChessGame::isValidMove(PieceColor turn, const Move &move,
                            const Board &board) const {
    if (!withinBoard(move.oldRow, move.oldCol) ||
        !withinBoard(move.newRow, move.newCol)) {
        return false;
    }

    std::shared_ptr<Piece> piece = board.getPieceAt(move.oldRow, move.oldCol);

    if (piece == nullptr || !piece->isMoveValid(move) ||
        piece->getColor() != turn) {
        return false;
    }

    std::shared_ptr<Piece> nextPiece =
        board.getPieceAt(move.newRow, move.newCol);

    // can't capture a piece of player's own color; also checks for self moves
    if (nextPiece != nullptr && piece->getColor() == nextPiece->getColor()) {
        return false;
    }

    int dy = abs(move.newRow - move.oldRow);
    int dx = abs(move.newCol - move.oldCol);

    // all pieces that can move more than one cell at a time in a direction
    // excludes knights that can "jump" over other pieces
    if (std::dynamic_pointer_cast<Bishop>(piece) ||
        std::dynamic_pointer_cast<Queen>(piece) ||
        std::dynamic_pointer_cast<Rook>(piece) ||
        std::dynamic_pointer_cast<Pawn>(piece)) {
        int i = move.oldRow;
        int j = move.oldCol;

        while (i != move.newRow || j != move.newCol) {
            if (i == move.oldRow && j == move.oldCol) {
                i += dy == 0 ? 0 : (move.newRow - move.oldRow) / dy;
                j += dx == 0 ? 0 : (move.newCol - move.oldCol) / dx;
                continue;
            }

            if (board.getPieceAt(i, j) != nullptr) {
                return false;
            }

            i += dy == 0 ? 0 : (move.newRow - move.oldRow) / dy;
            j += dx == 0 ? 0 : (move.newCol - move.oldCol) / dx;
        }

        if (std::dynamic_pointer_cast<Pawn>(piece) && dx == 0 &&
            board.getPieceAt(move.newRow, move.newCol) != nullptr) {
            return false;
        }
    }

    if (std::dynamic_pointer_cast<Pawn>(piece)) {
        // check that two cell advance only occurs on first move
        if (dy > 1 && piece->getMoveCount() > 0) {
            return false;
        }

        // check that only a capture move may be diagonal
        if (dy > 0 && dx > 0 && nextPiece == nullptr) {
            // special case for en passant
            int neighborCol = move.oldCol + (move.newCol - move.oldCol) / dx;
            if (withinBoard(move.oldRow, neighborCol)) {
                std::shared_ptr<Piece> neighborPiece =
                    board.getPieceAt(move.oldRow, neighborCol);

                // need to check that piece beside it is a pawn, and has
                // advanced two positions in the opponent's last turn
                if (std::dynamic_pointer_cast<Pawn>(neighborPiece) &&
                    moveList.size() > 0) {
                    Move lastMove = moveList.back();

                    if (lastMove.newRow == move.oldRow &&
                        lastMove.newCol == neighborCol &&
                        abs(lastMove.newRow - lastMove.oldRow) == 2) {
                        return true;
                    }
                }
            }

            return false;
        }
    }

    if (std::dynamic_pointer_cast<King>(piece) && dx == 2) {
        bool direction = (move.newCol - move.oldCol) < 0;
        std::shared_ptr<Piece> nextPiece =
            turn == White
                ? (direction ? board.getPieceAt(7, 0) : board.getPieceAt(7, 7))
                : (direction ? board.getPieceAt(0, 0) : board.getPieceAt(0, 7));

        // check that rook exists
        if (nextPiece == nullptr ||
            !std::dynamic_pointer_cast<Rook>(nextPiece)) {
            return false;
        }

        // check that king and rook haven't moved
        if (piece->getMoveCount() > 0 || nextPiece->getMoveCount() > 0) {
            return false;
        }

        // check that there are no pieces in between
        for (int j = move.oldCol; j != nextPiece->getCol();
             j += (move.newCol - move.oldCol) / dx) {
            if (j != move.oldCol && j != nextPiece->getCol() &&
                board.getPieceAt(move.oldRow, j) != nullptr) {
                return false;
            }
        }
    }

    return true;
}

bool ChessGame::isCaptureInternal(const Move &move, const Board &board) const {
    return board.getPieceAt(move.newRow, move.newCol) != nullptr;
}

bool ChessGame::isCaptureTargetInternal(const Move &move,
                                        std::pair<int, int> target) const {
    return move.newRow == target.first && move.newCol == target.second;
}

bool ChessGame::isCheckInternal(const Move &move,
                                std::pair<int, int> king) const {
    return move.newRow == king.first && move.newCol == king.second;
}

std::vector<Move>
ChessGame::generateLegalMovesInternal(PieceColor color,
                                      const Board &board) const {
    std::vector<Move> legalMoves = {};

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            for (int k = 0; k < 8; ++k) {
                for (int l = 0; l < 8; ++l) {
                    Move move = Move{i, j, k, l, color};
                    if (isValidMove(color, move, board)) {
                        legalMoves.push_back(move);
                    }
                }
            }
        }
    }

    return legalMoves;
}

void ChessGame::startGame() {
    gameState = Ongoing;
    moveList.clear();
}

void ChessGame::changeTurn() { turn = turn == White ? Black : White; }

bool ChessGame::moveInternal(const Move &move) {
    if (!isValidMove(turn, move, *board)) {
        return false;
    }

    std::shared_ptr<Piece> piece = board->getPieceAt(move.oldRow, move.oldCol);

    std::shared_ptr<Piece> nextPiece =
        board->getPieceAt(move.newRow, move.newCol);

    int dy = abs(move.newRow - move.oldRow);
    int dx = abs(move.newCol - move.oldCol);

    Board preliminaryBoard = Board(*board);

    // en passant
    if (std::dynamic_pointer_cast<Pawn>(piece) && dy > 0 && dx > 0 &&
        nextPiece == nullptr) {
        preliminaryBoard.move(move);

        if (isKingInCheckInternal(turn, preliminaryBoard)) {
            return false;
        }

        board->enPassantMove(move);

        moveList.push_back(move);

        // castling
    } else if (std::dynamic_pointer_cast<King>(piece) && dx == 2) {
        // initial position
        if (isKingInCheckInternal(turn, preliminaryBoard)) {
            return false;
        }

        Move intermediate =
            Move{move.oldRow, move.oldCol, move.newRow,
                 move.oldCol + (move.newCol - move.oldCol) / dx};
        preliminaryBoard.move(intermediate);

        // intermediate position
        if (isKingInCheckInternal(turn, preliminaryBoard)) {
            return false;
        }

        Move final =
            Move{intermediate.newRow, intermediate.newCol, intermediate.newRow,
                 intermediate.newCol + (move.newCol - move.oldCol) / dx};
        preliminaryBoard.move(final);

        // final position
        if (isKingInCheckInternal(turn, preliminaryBoard)) {
            return false;
        }

        bool direction = (move.newCol - move.oldCol) < 0;
        std::pair<int, int> rook =
            turn == White
                ? (direction ? std::make_pair(7, 0) : std::make_pair(7, 7))
                : (direction ? std::make_pair(0, 0) : std::make_pair(0, 7));

        Move rookMove = Move{rook.first, rook.second, rook.first,
                             move.oldCol + (move.newCol - move.oldCol) / dx};

        board->move(move);
        board->move(rookMove);

        moveList.push_back(move);

    } else {
        preliminaryBoard.move(move);

        // check if move puts king in check
        if (isKingInCheckInternal(turn, preliminaryBoard)) {
            return false;
        }

        board->move(move);

        moveList.push_back(move);
    }

    // reset game state to ongoing if escaped check
    if (gameState == (turn == White ? CheckForBlack : CheckForWhite)) {
        gameState = Ongoing;
    }

    return true;
}

bool ChessGame::move(const Move &move) { 
    if(moveInternal(move)) {
        computeState(turn == White ? Black : White);
        return true;
    }

    return false;
}

bool ChessGame::movePromotion(const Move &move,
                              std::shared_ptr<Piece> promotedPiece) {
    if (moveInternal(move)) {
        board->promotionMove(move, promotedPiece);
        computeState(turn == White ? Black : White);
        return true;
    }

    return false;
}

void ChessGame::resign() {
    gameState = turn == White ? ResignedWhite : ResignedBlack;
}

GameState ChessGame::getState() const { return gameState; }

PieceColor ChessGame::getTurn() const { return turn; }

std::vector<Move> ChessGame::generateLegalMoves() const {
    std::vector<Move> legalMoves =
        turn == White ? generateLegalMovesInternal(White, *board)
                      : generateLegalMovesInternal(Black, *board);

    std::vector<Move> nonCheckLegalMoves = {};

    // remove moves that put king into check
    for (const auto &m : legalMoves) {
        std::shared_ptr<Piece> piece = board->getPieceAt(m.oldRow, m.oldCol);

        std::shared_ptr<Piece> nextPiece =
            board->getPieceAt(m.newRow, m.newCol);

        // int dy = abs(m.newRow - m.oldRow);
        int dx = abs(m.newCol - m.oldCol);

        Board preliminaryBoard = Board(*board);

        // castling
        if (std::dynamic_pointer_cast<King>(piece) && dx == 2) {
            // initial position
            if (isKingInCheckInternal(turn, preliminaryBoard)) {
                continue;
            }

            Move intermediate = Move{m.oldRow, m.oldCol, m.newRow,
                                     m.oldCol + (m.newCol - m.oldCol) / dx};
            preliminaryBoard.move(intermediate);

            // intermediate position
            if (isKingInCheckInternal(turn, preliminaryBoard)) {
                continue;
            }

            Move final = Move{intermediate.newRow, intermediate.newCol,
                              intermediate.newRow,
                              intermediate.newCol + (m.newCol - m.oldCol) / dx};
            preliminaryBoard.move(final);

            // final position
            if (isKingInCheckInternal(turn, preliminaryBoard)) {
                continue;
            }

        } else {
            preliminaryBoard.move(m);

            // check if move puts king in check
            if (isKingInCheckInternal(turn, preliminaryBoard)) {
                continue;
            }
        }

        nonCheckLegalMoves.push_back(m);
    }

    return nonCheckLegalMoves;
}

void ChessGame::setPlayers(std::shared_ptr<Player> white,
                           std::shared_ptr<Player> black) {
    this->whitePlayer = white;
    this->blackPlayer = black;
}

void ChessGame::setTurn(PieceColor turn) { this->turn = turn; }

bool ChessGame::isCapture(const Move &move) const {
    return isCaptureInternal(move, *board);
}

bool ChessGame::isCheck(const Move &move) const {
    Board preliminaryBoard = Board(*board);
    preliminaryBoard.move(move);

    return isKingInCheckInternal(move.color == White ? Black : White, preliminaryBoard);
}

bool ChessGame::isMoveSafe(const Move &move) const {
    Board testBoard = Board(*board);
    testBoard.move(move);

    std::vector<Move> opponentMoves = generateLegalMovesInternal(
        move.color == White ? Black : White, testBoard);

    for (const auto &m : opponentMoves) {
        if (isCaptureTargetInternal(m,
                                    std::make_pair(move.newRow, move.newCol))) {
            return false;
        }
    }

    return true;
}

bool ChessGame::isPromotion(const Move &move) const {
    if (!withinBoard(move.oldRow, move.oldCol) ||
        !withinBoard(move.newRow, move.newCol)) {
        return false;
    }

    std::shared_ptr<Piece> piece = board->getPieceAt(move.oldRow, move.oldCol);

    if (piece == nullptr || !piece->isMoveValid(move) ||
        piece->getColor() != turn) {
        return false;
    }

    std::shared_ptr<Piece> nextPiece =
        board->getPieceAt(move.newRow, move.newCol);

    // can't capture a piece of player's own color; also checks for self moves
    if (nextPiece != nullptr && piece->getColor() == nextPiece->getColor()) {
        return false;
    }

    return std::dynamic_pointer_cast<Pawn>(piece) && (move.newRow == 7 ||
           move.newRow == 0);
}

bool ChessGame::kingIsInCheck(PieceColor kingColor) const {
    return isKingInCheckInternal(kingColor, *board);
}

int ChessGame::evaluateBoard(PieceColor color) const {
    int materialScore = 0;

    // Define piece values
    std::map<char, int> pieceValues = {{'k', 10000}, {'q', 900}, {'r', 500},
                                       {'b', 330},   {'n', 320}, {'p', 100}};

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const std::shared_ptr<Piece> piece = board->getPieceAt(row, col);
            if (piece != nullptr) {
                int pieceValue = pieceValues[piece->getLetter()];
                if (piece->getColor() == color) {
                    materialScore += pieceValue;
                } else {
                    materialScore -= pieceValue;
                }
            }
        }
    }

    return materialScore;
}