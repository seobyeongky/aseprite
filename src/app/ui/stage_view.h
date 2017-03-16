// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_STAGE_VIEW_H_INCLUDED
#define APP_UI_STAGE_VIEW_H_INCLUDED
#pragma once

#include "app/app_render.h"
#include "app/ui/tabs.h"
#include "app/ui/workspace_view.h"
#include "app/ui/editor/editor.h"
#include "ui/box.h"
#include "app/pref/preferences.h"

#include "stage_view.xml.h"

namespace ui {
  class View;
}
namespace doc{
  class Palette;
}

namespace app {
  using namespace doc;

  class StageView : public app::gen::StageView
                 , public TabView
                 , public WorkspaceView
  {
  public:
    StageView();
    ~StageView();

    void updateUsingEditor(Editor* editor);

    // TabView implementation
    std::string getTabText() override;
    TabIcon getTabIcon() override;

    // WorkspaceView implementation
    ui::Widget* getContentWidget() override { return this; }
    bool onCloseView(Workspace* workspace, bool quitting) override;
    void onTabPopup(Workspace* workspace) override;
    void onWorkspaceViewSelected() override;

  protected:
    void onPaint(ui::PaintEvent& ev) override;
    void onResize(ui::ResizeEvent& ev) override;

  private:
    static AppRender m_renderEngine;

    Editor* m_relatedEditor;
    Document* m_doc;
    // Extra space around the sprite.
    gfx::Point m_padding;
    render::Projection m_proj;    // Zoom/pixel ratio in the editor
    Image* m_doublebuf;
    she::Surface* m_doublesur;

    gfx::Point m_pos;
    gfx::Point m_oldMousePos;
    gfx::Point m_delta;

    Palette* m_bgPal;
    DocumentPreferences m_docPref;

    void drawBG(ui::PaintEvent& ev);
    void drawOneSpriteUnclippedRect(ui::Graphics* g
      , const gfx::Rect& spriteRectToDraw
      , int dx
      , int dy
      , Sprite* sprite);
  };

} // namespace app

#endif
