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
#include "defs.h"
#include "board.h"
#include "move.h"
#include "evaluate.h"
#include "history.h"
#include <cassert>

enum {
  STAGE_NONE = 0, STAGE_HASHMOVE, STAGE_GEN_NOISY, STAGE_GOOD_NOISY,
  STAGE_KILLER_1, STAGE_KILLER_2, STAGE_COUNTER, STAGE_GEN_QUIETS,
  STAGE_QUIETS, STAGE_BAD_NOISY, STAGE_DONE
}; /// move picker stages

bool see(Board &board, uint16_t move, int threshold);

const int seeVal[] = {0, 100, 310, 330, 500, 1000, 0};

class Movepick {
public:
  int stage;

  uint16_t hashMove, killer1, killer2, counter, possibleCounter;
  int nrNoisy, nrQuiets, nrBadNoisy;

  int threshold;

  uint16_t noisy[256], quiets[256], badNoisy[256];
  int scores[256];

  Movepick(const uint16_t HashMove, const uint16_t Killer1, const uint16_t Killer2, const uint16_t Counter, const int Threshold) {
    stage = STAGE_HASHMOVE;

    hashMove = HashMove;
    killer1 = Killer1;
    killer2 = Killer2;
    counter = Counter;

    nrNoisy = nrQuiets = nrBadNoisy = 0;
    threshold = Threshold;
  }

  int getBestMoveInd(int nrMoves, int start) {
    int ind = start;

    for(int i = start + 1; i < nrMoves; i++) {
      if(scores[i] > scores[ind])
        ind = i;
    }

    return ind;
  }

  uint16_t nextMove(Search *searcher, bool skip, bool noisyPicker) {

    if(stage == STAGE_HASHMOVE) {
      stage++;
      if(hashMove) {
        return hashMove;
      }
    }

    if(stage == STAGE_GEN_NOISY) {
      /// good noisy
      nrNoisy = genLegalNoisy(searcher->board, noisy);

      for(int i = 0; i < nrNoisy; i++) {
        uint16_t move = noisy[i];
        int p = searcher->board.piece_type_at(sqFrom(move)), cap = searcher->board.piece_type_at(sqTo(move)), score;
        if(type(move) == ENPASSANT)
          cap = PAWN;
        score = captureValue[p][cap];
        if(type(move) == PROMOTION)
          score += 100 * (promoted(move) + KNIGHT);

        scores[i] = score;
      }
      stage++;
    }

    if(stage == STAGE_GOOD_NOISY) {

      if(nrNoisy) {

        int ind = getBestMoveInd(nrNoisy, 0);

        uint16_t best = noisy[ind];

        if(scores[ind] >= 0) {

          if(!see(searcher->board, best, threshold)) {
            badNoisy[nrBadNoisy++] = best;
            scores[ind] = -1; /// mark capture as bad
            return nextMove(searcher, skip, noisyPicker);
          }

          nrNoisy--;
          noisy[ind] = noisy[nrNoisy];
          scores[ind] = scores[nrNoisy];

          if(best == hashMove) /// don't play the same move
            return nextMove(searcher, skip, noisyPicker);

          if(best == killer1)
            killer1 = NULLMOVE;
          if(best == killer2)
            killer2 = NULLMOVE;
          if(best == counter)
            counter = NULLMOVE;

          return best;
        }
      }
      if(skip) { /// no need to go through quiets
        stage = STAGE_BAD_NOISY;
        return nextMove(searcher, skip, noisyPicker);
      }
      stage++;
    }

    if(stage == STAGE_KILLER_1) {

      stage++;

      if(!skip && killer1 && killer1 != hashMove && isLegalMove(searcher->board, killer1))
        return killer1;
    }

    if(stage == STAGE_KILLER_2) {

      stage++;

      if(!skip && killer2 && killer2 != hashMove && isLegalMove(searcher->board, killer2))
        return killer2;
    }

    if(stage == STAGE_COUNTER) {

      stage++;

      if(!skip && counter && counter != hashMove && counter != killer1 && counter != killer2 && isLegalMove(searcher->board, counter))
        return counter;
    }

    if(stage == STAGE_GEN_QUIETS) {
      /// quiet moves
      /// TO DO: don't generate all quiets to validate refutation moves, add fast isLegal(move) function ? - done

      nrQuiets = genLegalQuiets(searcher->board, quiets);

      int ply = searcher->board.ply;

      uint16_t counterMove = (ply >= 1 ? searcher->Stack[ply - 1].move : NULLMOVE), followMove = (ply >= 2 ? searcher->Stack[ply - 2].move : NULLMOVE);
      int counterPiece = (ply >= 1 ? searcher->Stack[ply - 1].piece : 0), followPiece = (ply >= 2 ? searcher->Stack[ply - 2].piece : 0);
      int counterTo = sqTo(counterMove), followTo = sqTo(followMove);

      for(int i = 0; i < nrQuiets; i++) {
        uint16_t move = quiets[i];
        int score = 0;

        if(move == hashMove || move == killer1 || move == killer2 || move == counter)
          score = -1000000000;
        else {
          int from = sqFrom(move), to = sqTo(move), piece = searcher->board.board[from];

          score = searcher->hist[searcher->board.turn][from][to];

          if(counterMove)
            score += searcher->follow[0][counterPiece][counterTo][piece][to];

          if(followMove)
            score += searcher->follow[1][followPiece][followTo][piece][to];

          score += searcher->nodesSearched[from][to] / 10000; // the longer it takes a move to be refuted, the higher its chance to become the best move
        }
        scores[i] = score;
      }

      stage++;
    }

    if(stage == STAGE_QUIETS) {

      if(!skip && nrQuiets) {

        int ind = getBestMoveInd(nrQuiets, 0);

        uint16_t best = quiets[ind];

        nrQuiets--;
        quiets[ind] = quiets[nrQuiets];
        scores[ind] = scores[nrQuiets];

        if(best == hashMove || best == killer1 || best == killer2 || best == counter)
          return nextMove(searcher, skip, noisyPicker);

        return best;
      } else {
        stage++;
      }
    }

    if(stage == STAGE_BAD_NOISY) {
      /// bad noisy moves
      if(nrBadNoisy && !noisyPicker) { /// bad captures can't improve the current position in quiesce search
        nrBadNoisy--;
        uint16_t move = badNoisy[nrBadNoisy];

        if(move == hashMove || move == killer1 || move == killer2 || move == counter)
          return nextMove(searcher, skip, noisyPicker);

        return move;
      } else {
        stage++;
      }
    }

    if(stage == STAGE_DONE) {
      return NULLMOVE;
    }

    assert(0);

    return NULLMOVE;
  }
};



bool see(Board &board, uint16_t move, int threshold) {
  int from = sqFrom(move), to = sqTo(move), t = type(move), col, nextVictim, score = -threshold;
  uint64_t diag, orth, occ, att, myAtt;

  nextVictim = (t != PROMOTION ? piece_type(board.board[from]) : promoted(move) + KNIGHT);

  score += seeVal[piece_type(board.board[to])];

  if(t == PROMOTION)
    score += seeVal[promoted(move) + KNIGHT] - seeVal[PAWN];
  else if(t == ENPASSANT)
    score = seeVal[PAWN];

  if(score < 0)
    return 0;

  score -= seeVal[nextVictim];

  if(score >= 0)
    return 1;

  diag = board.diagSliders(WHITE) | board.diagSliders(BLACK);
  orth = board.orthSliders(WHITE) | board.orthSliders(BLACK);

  occ = board.pieces[WHITE] | board.pieces[BLACK];
  occ = (occ ^ (1ULL << from)) | (1ULL << to);

  if(t == ENPASSANT)
    occ ^= (1ULL << board.enPas);

  att = (getAttackers(board, WHITE, occ, to) | getAttackers(board, BLACK, occ, to)) & occ;

  col = 1 ^ board.turn;

  while(true) {
    myAtt = att & board.pieces[col];

    if(!myAtt)
      break;

    for(nextVictim = PAWN; nextVictim <= QUEEN; nextVictim++) {
      if(myAtt & board.bb[getType(nextVictim, col)])
        break;
    }

    occ ^= (1ULL << Sq(lsb(myAtt & board.bb[getType(nextVictim, col)])));

    if(nextVictim == PAWN || nextVictim == BISHOP || nextVictim == QUEEN)
      att |= genAttacksBishop(occ, to) & diag;

    if(nextVictim == ROOK || nextVictim == QUEEN)
      att |= genAttacksRook(occ, to) & orth;

    att &= occ;
    col ^= 1;

    score = -score - 1 - seeVal[nextVictim];

    if(score >= 0) {
      if(nextVictim == KING && (att & board.pieces[col]))
        col ^= 1;

      break;
    }
  }

  return board.turn != col;
}
