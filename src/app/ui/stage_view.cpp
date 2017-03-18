// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/stage_view.h"

#include "app/app.h"
#include "app/app_menus.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/ui/main_window.h"
#include "app/ui/skin/skin_style_property.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/workspace.h"
#include "app/ui/workspace_tabs.h"
#include "app/ui_context.h"
#include "app/ui/editor/editor.h"
#include "app/ui/stage_editor.h"
#include "base/bind.h"
#include "base/exception.h"
#include "ui/label.h"
#include "ui/resize_event.h"
#include "ui/system.h"
#include "ui/textbox.h"
#include "ui/view.h"
#include "app/context_access.h"
#include "app/document_access.h"
#include "app/document_range.h"
#include "doc/document_event.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/sprite.h"
#include "gfx/size.h"
#include "app/ui/editor/editor.h"
#include "app/console.h"
#include "she/surface.h"
#include "she/system.h"
#include "doc/conversion_she.h"
#include "doc/palette.h"
#include "app/color_utils.h"
#include "app/ui/timeline.h"
#include "app/ui/editor/moving_pixels_state.h"
#include "app/ui/editor/pixels_movement.h"
#include "app/ui/editor/play_state.h"
#include "app/ui/editor/scrolling_state.h"
#include "app/ui/editor/standby_state.h"
#include "app/ui/editor/zooming_state.h"
#include "app/ui/status_bar.h"
#include "app/modules/gui.h"
#include "doc/site.h"
#include "app/modules/playables.h"


namespace app {

using namespace ui;
using namespace app::skin;


StageView::StageView()
  : m_stageEditor(new StageEditor())
{
  m_dbgLabel = new Label("debug");
  dbgBox()->addChild(m_dbgLabel);

  m_positionLabel = new Label("sfdasdf");
  m_positionLabel->setExpansive(true);
  dbgBox()->addChild(m_positionLabel);

  m_stageEditor->setVisible(true);
  stageEditorView()->attachToView(m_stageEditor);
  stageEditorView()->setExpansive(true);

  SkinTheme* theme = static_cast<SkinTheme*>(this->theme());
  int barsize = theme->dimensions.miniScrollbarSize();

  stageEditorView()->horizontalBar()->setBarWidth(barsize);
  stageEditorView()->verticalBar()->setBarWidth(barsize);

  setup_mini_look(stageEditorView()->horizontalBar());
  setup_mini_look(stageEditorView()->verticalBar());

  stageEditorView()->showScrollBars();
}

StageView::~StageView()
{
  delete m_positionLabel;
  delete m_dbgLabel;
  delete m_stageEditor;
}

void StageView::updateUsingEditor(Editor* editor)
{
  if (editor == nullptr || !editor->isActive())
  {
    if (isVisible())
      App::instance()->mainWindow()->getTimeline()->updateUsingStageEditor(m_stageEditor);
    return;
  }

  m_stageEditor->setDocument(editor->document());
}

void StageView::getSite(Site* site)
{
  m_stageEditor->getSite(site);
}

std::string StageView::getTabText()
{
  return "Stage";
}

TabIcon StageView::getTabIcon()
{
  return TabIcon::STAGE;
}

bool StageView::onCloseView(Workspace* workspace, bool quitting)
{
  workspace->removeView(this);
  return true;
}

void StageView::onTabPopup(Workspace* workspace)
{
  Menu* menu = AppMenus::instance()->getTabPopupMenu();
  if (!menu)
    return;

  menu->showPopup(ui::get_mouse_position());
}

void StageView::onWorkspaceViewSelected()
{
}

void StageView::onResize(ui::ResizeEvent& ev)
{
  ui::VBox::onResize(ev);
}

void StageView::onVisible(bool visible)
{
  if (visible) {
    current_playable = m_stageEditor;
  }
}

} // namespace app
