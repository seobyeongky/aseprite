// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_TIMELINE_ANI_CONTROLS_H_INCLUDED
#define APP_UI_TIMELINE_ANI_CONTROLS_H_INCLUDED
#pragma once

#include "app/ui/button_set.h"
#include "ui/widget.h"

#include <string>
#include <vector>

namespace app {
  class Editor;
  class Playable;

  class AniControls : public ButtonSet {
  public:
    AniControls();

    void updateUsingPlayable(Playable* playable);

  protected:
    void onRightClick(Item* item) override;

  private:
    void onClickButton();

    const char* getCommandId(int index) const;
    std::string getTooltipFor(int index) const;
  };

} // namespace app

#endif