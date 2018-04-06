// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_DATA_RECOVERY_VIEW_H_INCLUDED
#define APP_UI_DATA_RECOVERY_VIEW_H_INCLUDED
#pragma once

#include "app/crash/raw_images_as.h"
#include "app/ui/drop_down_button.h"
#include "app/ui/tabs.h"
#include "app/ui/workspace_view.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/listbox.h"
#include "ui/view.h"

namespace app {
  namespace crash {
    class DataRecovery;
  }

  class DataRecoveryView : public ui::VBox
                         , public TabView
                         , public WorkspaceView {
  public:
    DataRecoveryView(crash::DataRecovery* dataRecovery);

    // TabView implementation
    std::string getTabText() override;
    TabIcon getTabIcon() override;

    // WorkspaceView implementation
    ui::Widget* getContentWidget() override { return this; }
    void onWorkspaceViewSelected() override;
    bool onCloseView(Workspace* workspace, bool quitting) override;
    void onTabPopup(Workspace* workspace) override;

    // Triggered when the list is empty (because the user deleted all
    // sessions).
    obs::signal<void()> Empty;

  private:
    void fillList();

    void onOpen();
    void onOpenRaw(crash::RawImagesAs as);
    void onOpenMenu();
    void onDelete();
    void onChangeSelection();

    crash::DataRecovery* m_dataRecovery;
    ui::View m_view;
    ui::ListBox m_listBox;
    DropDownButton m_openButton;
    ui::Button m_deleteButton;
  };

} // namespace app

#endif
