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
#include <cstring>
#include "attacks.h"
#include "defs.h"
#include "board.h"
#include "thread.h"

const int TEMPO = 20;

int evaluate(Board& board) {

    //float eval2 = board.NN.getOutput();

    //board.print();

    /*NetInput input = board.toNetInput();
    float score = board.NN.calc(input);

    if (abs(eval2 - score) > 1e-1) {
      board.print();
      std::cout << eval2 << " " << score << "\n";
      assert(0);
    }*/

    return int(board.NN.getOutput()) * (board.turn == WHITE ? 1 : -1) + TEMPO;
}
