#pragma once
#include "player/fo3-player-state.h"
#include "ui/pipboy/fo3-pipboy-state.h"
namespace fo3barter {
// Native barter exposes All plus the same five inventory categories. This is
// presentation selection only; Player remains the transaction authority.
inline bool InCategory(const fo3player::Item &item, int filter) {
  return filter < 0 || fo3pip::Category(item.kind) == filter;
}
struct Controls {
  int filters[2]{-1, -1};
  uint64_t quantityStack = 0;
  bool quantityBuying = false;
  int32_t quantity = 1, maximum = 1;
  bool leftDown = false, rightDown = false, xDown = false;
  void CancelQuantity() { quantityStack = 0; }
  bool Editing() const { return quantityStack != 0; }
  void Set(fo3player::BarterSession &session, uint64_t stack, bool buying,
           int32_t count) {
    auto &choices = session.selections;
    choices.erase(std::remove_if(choices.begin(), choices.end(),
                                 [&](const auto &s) {
                                   return s.stack == stack &&
                                          s.buying == buying;
                                 }),
                  choices.end());
    if (count > 0)
      choices.push_back({stack, count, buying});
  }
  void Begin(fo3player::BarterSession &session, uint64_t id, bool buying) {
    for (const auto &stack : buying ? session.merchant : session.player)
      if (stack.id == id) {
        auto old = std::find_if(
            session.selections.begin(), session.selections.end(),
            [&](const auto &s) { return s.stack == id && s.buying == buying; });
        if (old != session.selections.end()) {
          Set(session, id, buying, 0);
          return;
        }
        if (stack.count == 1) {
          Set(session, id, buying, 1);
          return;
        }
        quantityStack = id;
        quantityBuying = buying;
        quantity = maximum = stack.count;
        return;
      }
  }
  void Adjust(int delta) {
    quantity =
        int32_t(std::clamp<int64_t>(int64_t(quantity) + delta, 1, maximum));
  }
  void Accept(fo3player::BarterSession &session) {
    if (Editing())
      Set(session, quantityStack, quantityBuying, quantity);
    CancelQuantity();
  }
};
} // namespace fo3barter
