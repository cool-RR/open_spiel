// Copyright 2019 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/games/mancala.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "open_spiel/spiel_utils.h"
#include "open_spiel/utils/tensor_view.h"

namespace open_spiel {
namespace mancala {
namespace {

// Facts about the game.
const GameType kGameType{
    /*short_name=*/"mancala",
    /*long_name=*/"Mancala",
    GameType::Dynamics::kSequential,
    GameType::ChanceMode::kDeterministic,
    GameType::Information::kPerfectInformation,
    GameType::Utility::kZeroSum,
    GameType::RewardModel::kTerminal,
    /*max_num_players=*/2,
    /*min_num_players=*/2,
    /*provides_information_state_string=*/true,
    /*provides_information_state_tensor=*/false,
    /*provides_observation_string=*/true,
    /*provides_observation_tensor=*/true,
    /*parameter_specification=*/{}  // no parameters
};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new MancalaGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

}  // namespace

int MancalaState::GetPlayerHomePit(Player player) const {
  if(player == 0){
    return kTotalPits / 2;
  }
  return 0;
}

void MancalaState::DoApplyAction(Action move) {
  // SPIEL_CHECK_EQ(board_[move], CellState::kEmpty);
  int num_beans = board_[move];
  board_[move] = 0;
  for(int i = 0; i < num_beans; ++i){
    board_[(move + i + 1) % kTotalPits]++;
  }
  if((move + num_beans) % kTotalPits != GetPlayerHomePit(current_player_))
    current_player_ = 1 - current_player_;
  num_moves_ += 1;
}

std::vector<Action> MancalaState::LegalActions() const {
  if (IsTerminal()) return {};
  std::vector<Action> moves;
  if(current_player_ == 0){
    for(int i = 0; i < kNumPits; ++i){
      if(board_[i + 1] > 0){
        moves.push_back(i + 1);
      }
    }
  } else {
    for(int i = 0; i < kNumPits; ++i){
      if(board_[board_.size() - 1 - i] > 0){
        moves.push_back(board_.size() - 1 - i);
      }
    }
  }
  return moves;
}

std::string MancalaState::ActionToString(Player player,
                                           Action action_id) const {
  return absl::StrCat(action_id);
}


bool MancalaState::IsFull() const { return num_moves_ == kNumCells; }

void MancalaState::InitBoard() {
  std::fill(begin(board_), end(board_), 4);
  board_[0] = 0;
  board_[board_.size() / 2] = 0;
}

MancalaState::MancalaState(std::shared_ptr<const Game> game) : State(game) {
  InitBoard();
}

std::string MancalaState::ToString() const {
  std::string str;
  std::string separator = "-";
  absl::StrAppend(&str, separator);
  for (int i = 0; i < kNumPits; ++i) {
    absl::StrAppend(&str, board_[board_.size() - 1 - i]);
    absl::StrAppend(&str, separator);
  }
  absl::StrAppend(&str, "\n");

  
  absl::StrAppend(&str, board_[0]);
  for (int i = 0; i < kNumPits * 2 - 1; ++i) {
    absl::StrAppend(&str, separator);
  }
  absl::StrAppend(&str, board_[board_.size() / 2]);
  absl::StrAppend(&str, "\n");


  absl::StrAppend(&str, separator);
  for (int i = 0; i < kNumPits; ++i) {
    absl::StrAppend(&str, board_[i + 1]);
    absl::StrAppend(&str, separator);
  }
  return str;
}

bool MancalaState::IsTerminal() const {
  bool player_0_has_moves = false;
  bool player_1_has_moves = false;
  for (int i = 0; i < kNumPits; ++i) {
    if(board_[board_.size() - 1 - i] > 0){
      player_1_has_moves = true;
      break;
    }    
  }
  for (int i = 0; i < kNumPits; ++i) {
    if(board_[i + 1] > 0){
      player_0_has_moves = true;
      break;
    }    
  }
  return !player_0_has_moves || !player_1_has_moves;
}

std::vector<double> MancalaState::Returns() const {
  int player_0_bean_sum = std::accumulate(board_.begin() + 1, board_.begin() + kTotalPits / 2 + 1, 0);
  int player_1_bean_sum = std::accumulate(board_.begin() + kTotalPits / 2 + 1, board_.end(), 0) + board_[0];
  if (player_0_bean_sum > player_1_bean_sum) {
    // return {player_0_bean_sum, player_1_bean_sum};
    return {1.0, -1.0};
  } else if (player_0_bean_sum < player_1_bean_sum) {
    // return {player_0_bean_sum, player_1_bean_sum};
    return {-1.0, 1.0};
  } else {
    // return {player_0_bean_sum, player_1_bean_sum};
    return {0.0, 0.0};
  }
}

std::string MancalaState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return HistoryString();
}

std::string MancalaState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return ToString();
}

void MancalaState::ObservationTensor(Player player,
                                       absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);

  // Treat `values` as a 2-d tensor.
  TensorView<2> view(values, {kCellStates, kNumCells}, true);
  for (int cell = 0; cell < kNumCells; ++cell) {
    view[{static_cast<int>(board_[cell]), cell}] = 1.0;
  }
}

void MancalaState::UndoAction(Player player, Action move) {
  // board_[move] = CellState::kEmpty;
  // current_player_ = player;
  // outcome_ = kInvalidPlayer;
  num_moves_ -= 1;
  history_.pop_back();
  --move_number_;
}

std::unique_ptr<State> MancalaState::Clone() const {
  return std::unique_ptr<State>(new MancalaState(*this));
}

MancalaGame::MancalaGame(const GameParameters& params)
    : Game(kGameType, params) {}

}  // namespace mancala
}  // namespace open_spiel
