// Aseprite
// Copyright (C) 2016-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_EDITOR_GLUE_H_INCLUDED
#define APP_UI_EDITOR_GLUE_H_INCLUDED
#pragma once

#include "app/tools/pointer.h"
#include "app/ui/editor/editor.h"
#include "ui/message.h"

namespace app {

// Code to convert "ui" messages to "tools" mouse pointers

inline tools::Pointer::Button button_from_msg(const ui::MouseMessage* msg) {
  return
    (msg->right() ? tools::Pointer::Right:
     (msg->middle() ? tools::Pointer::Middle:
                      tools::Pointer::Left));
}

inline tools::Pointer pointer_from_msg(Editor* editor,
                                       const ui::MouseMessage* msg) {
  return
    tools::Pointer(editor->screenToEditor(msg->position()),
                   button_from_msg(msg));
}

} // namespace app

#endif
