// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_APP_MENUITEM_H_INCLUDED
#define APP_UI_APP_MENUITEM_H_INCLUDED
#pragma once

#include "app/commands/params.h"
#include "app/ui/key_context.h"
#include "she/shortcut.h"
#include "ui/menu.h"

namespace she {
  class MenuItem;
}

namespace app {
  class Key;
  class Command;

  // A widget that represent a menu item of the application.
  //
  // It's like a MenuItme, but it has a extra properties: the name of
  // the command to be executed when it's clicked (also that command is
  // used to check the availability of the command).
  class AppMenuItem : public ui::MenuItem {
  public:
    struct Native {
      she::MenuItem* menuItem = nullptr;
      she::Shortcut shortcut;
      app::KeyContext keyContext = app::KeyContext::Any;
    };

    AppMenuItem(const std::string& text,
                Command* command = nullptr,
                const Params& params = Params());
    ~AppMenuItem();

    Key* key() { return m_key; }
    void setKey(Key* key);

    Command* getCommand() { return m_command; }
    const Params& getParams() const { return m_params; }

    Native* native() { return m_native; }
    void setNative(const Native& native);
    void syncNativeMenuItemKeyShortcut();

    static void setContextParams(const Params& params);

  protected:
    bool onProcessMessage(ui::Message* msg) override;
    void onSizeHint(ui::SizeHintEvent& ev) override;
    void onClick() override;
    void onValidate() override;

  private:
    Key* m_key;
    Command* m_command;
    Params m_params;
    Native* m_native;

    static Params s_contextParams;
  };

} // namespace app

#endif
