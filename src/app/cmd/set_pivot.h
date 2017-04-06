#ifndef APP_CMD_SET_PIVOT_H_INCLUDED
#define APP_CMD_SET_PIVOT_H_INCLUDED
#pragma once

#include "app/cmd.h"
#include "app/cmd/with_sprite.h"

#include "gfx/point.h"

namespace doc {
  class Sprite;
}

namespace app {
namespace cmd {
  using namespace doc;

  class SetPivot : public Cmd
                      , public WithSprite {
  public:
    SetPivot(Sprite* sprite, gfx::PointF pivot);

  protected:
    void onExecute() override;
    void onUndo() override;
    void onFireNotifications() override;
    size_t onMemSize() const override {
      return sizeof(*this);
    }

  private:
    void setPivot(gfx::PointF pivot);

    gfx::PointF m_oldPivot;
    gfx::PointF m_newPivot;
  };

} // namespace cmd
} // namespace app

#endif
