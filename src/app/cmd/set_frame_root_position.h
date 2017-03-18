// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_CMD_SET_FRAME_POSITION_H_INCLUDED
#define APP_CMD_SET_FRAME_POSITION_H_INCLUDED
#pragma once

#include "app/cmd.h"
#include "app/cmd/with_sprite.h"
#include "doc/frame.h"
#include "gfx/point.h"

namespace app {
namespace cmd {
  using namespace doc;

  class SetFrameRootPosition : public Cmd
                             , public WithSprite {
  public:
    SetFrameRootPosition(Sprite* sprite, frame_t frame, const gfx::Point & p);

  protected:
    void onExecute() override;
    void onUndo() override;
    void onFireNotifications() override;
    size_t onMemSize() const override {
      return sizeof(*this);
    }

  private:
    frame_t m_frame;
    gfx::Point m_oldPosition;
    gfx::Point m_newPosition;
  };

} // namespace cmd
} // namespace app

#endif
