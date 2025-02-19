/*
  Clover is a UCI chess playing engine authored by Luca Metehau.
  <https://github.com/lucametehau/CloverEngine>

  Clover is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clover is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <vector>
#include "board.h"
#include "defs.h"
#include "thread.h"

class Search;

class History {
  History() = delete;
  ~History() = delete;

  public:
    struct Heuristics {
      Heuristics() : h(0), ch(0), fh(0) {}
      int h, ch, fh;
    };

  public:
    static void updateHistory(Search *searcher, uint16_t *quiets, int nrQuiets, int ply, int bonus);
    static void getHistory(Search *searcher, uint16_t move, int ply, Heuristics &H);
    static void updateHist(int &hist, int score);
  private:
    static constexpr int histMax = 400;
    static constexpr int histMult = 32;
    static constexpr int histDiv = 512;
};

constexpr int History :: histMax;
constexpr int History :: histMult;
constexpr int History :: histDiv;


void History :: updateHist(int &hist, int score) {
  hist += score * histMult - hist * abs(score) / histDiv;
}

void History :: updateHistory(Search *searcher, uint16_t *quiets, int nrQuiets, int ply, int bonus) {
  if(ply < 2 || !nrQuiets) /// we can't update if we don't have a follow move or no quiets
    return;

  uint16_t counterMove = searcher->Stack[ply - 1].move, followMove = searcher->Stack[ply - 2].move;
  int counterPiece = searcher->Stack[ply - 1].piece, followPiece = searcher->Stack[ply - 2].piece;
  int counterTo = sqTo(counterMove), followTo = sqTo(followMove);

  uint16_t best = quiets[nrQuiets - 1];
  bool turn = searcher->board.turn;

  if(counterMove)
    searcher->cmTable[1 ^ turn][counterPiece][counterTo] = best; /// update counter move table

  bonus = std::min(bonus, histMax);

  for(int i = 0; i < nrQuiets; i++) {
    /// increase value for best move, decrease value for the other moves
    /// so we have an early cut-off

    int move = quiets[i];
    int score = (move == best ? bonus : -bonus);
    int from = sqFrom(move), to = sqTo(move), piece = searcher->board.board[from];

    updateHist(searcher->hist[turn][from][to], score);

    if(counterMove)
      updateHist(searcher->follow[0][counterPiece][counterTo][piece][to], score);

    if(followMove)
      updateHist(searcher->follow[1][followPiece][followTo][piece][to], score);
  }
}

void History :: getHistory(Search *searcher, uint16_t move, int ply, Heuristics &H) {
  int from = sqFrom(move), to = sqTo(move), piece = searcher->board.board[from];

  H.h = searcher->hist[searcher->board.turn][from][to];

  uint16_t counterMove = searcher->Stack[ply - 1].move, followMove = (ply >= 2 ? searcher->Stack[ply - 2].move : NULLMOVE);
  int counterPiece = searcher->Stack[ply - 1].piece, followPiece = (ply >= 2 ? searcher->Stack[ply - 2].piece : 0);
  int counterTo = sqTo(counterMove), followTo = sqTo(followMove);

  H.ch = searcher->follow[0][counterPiece][counterTo][piece][to];

  H.fh = searcher->follow[1][followPiece][followTo][piece][to];
}
