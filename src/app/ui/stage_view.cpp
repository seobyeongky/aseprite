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


namespace app {

using namespace ui;
using namespace app::skin;

AppRender StageView::m_renderEngine;

StageView::StageView()
  : m_doublebuf(nullptr)
  , m_doublesur(nullptr)
  , m_bgPal(Palette::createGrayscale())
  , m_docPref("")
{
}

StageView::~StageView()
{
}

void StageView::updateUsingEditor(Editor* editor)
{
  if (editor == nullptr || !editor->isActive())
    return;

  m_relatedEditor = editor;
  m_doc = editor->document();
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

  if (m_doublebuf)
  {
    delete m_doublebuf;
  }
  m_doublebuf = Image::create(IMAGE_RGB, ev.bounds().w, ev.bounds().h);
  if (m_doublesur)
  {
    m_doublesur->dispose();
  }
  m_doublesur = she::instance()->createRgbaSurface(ev.bounds().w, ev.bounds().h);
}

void StageView::onPaint(ui::PaintEvent& ev)
{
  Graphics* g = ev.graphics();
  gfx::Rect rc = clientBounds();
  SkinTheme* theme = static_cast<SkinTheme*>(this->theme());

  drawBG(ev);

  if (m_doc == nullptr || m_doc->sprite() == nullptr)
  {
    return;
  }

  auto sprite = m_doc->sprite();

  drawOneSpriteUnclippedRect(g
    , gfx::Rect(0, 0, sprite->width(), sprite->height())
    , 100
    , 100
    , sprite);
}

void StageView::drawBG(ui::PaintEvent& ev)
{
  Graphics* g = ev.graphics();

  m_renderEngine.setRefLayersVisiblity(false);
  m_renderEngine.setProjection(render::Projection());
  m_renderEngine.disableOnionskin();
  m_renderEngine.setBgType(render::BgType::TRANSPARENT);
  m_renderEngine.setProjection(m_proj);

  render::BgType bgType;
  gfx::Size tile;
  switch (m_docPref.bg.type()) {
    case app::gen::BgType::CHECKED_16x16:
      bgType = render::BgType::CHECKED;
      tile = gfx::Size(16, 16);
      break;
    case app::gen::BgType::CHECKED_8x8:
      bgType = render::BgType::CHECKED;
      tile = gfx::Size(8, 8);
      break;
    case app::gen::BgType::CHECKED_4x4:
      bgType = render::BgType::CHECKED;
      tile = gfx::Size(4, 4);
      break;
    case app::gen::BgType::CHECKED_2x2:
      bgType = render::BgType::CHECKED;
      tile = gfx::Size(2, 2);
      break;
    default:
      bgType = render::BgType::TRANSPARENT;
      break;
  }

  m_renderEngine.setBgType(bgType);
  m_renderEngine.setBgZoom(m_docPref.bg.zoom());
  m_renderEngine.setBgColor1(color_utils::color_for_image(m_docPref.bg.color1(), m_doublebuf->pixelFormat()));
  m_renderEngine.setBgColor2(color_utils::color_for_image(m_docPref.bg.color2(), m_doublebuf->pixelFormat()));
  m_renderEngine.setBgCheckedSize(tile);

  //m_renderEngine.setupBackground(m_doc, m_doublebuf->pixelFormat());
  m_renderEngine.renderBackground(m_doublebuf,
    gfx::Clip(0, 0, -m_pos.x, -m_pos.y,
      m_doublebuf->width(), m_doublebuf->height()));

  convert_image_to_surface(m_doublebuf, m_bgPal,
    m_doublesur, 0, 0, 0, 0, m_doublebuf->width(), m_doublebuf->height());
  g->blit(m_doublesur, 0, 0, 0, 0, m_doublesur->width(), m_doublesur->height()); 
}

void StageView::drawOneSpriteUnclippedRect(ui::Graphics* g
  , const gfx::Rect& spriteRectToDraw
  , int dx
  , int dy
  , Sprite * sprite)
{
  // Clip from sprite and apply zoom
  gfx::Rect rc = sprite->bounds().createIntersection(spriteRectToDraw);
  rc = m_proj.apply(rc);

  int dest_x = dx + m_padding.x + rc.x;
  int dest_y = dy + m_padding.y + rc.y;

  // Clip from graphics/screen
  const gfx::Rect& clip = g->getClipBounds();
  if (dest_x < clip.x) {
    rc.x += clip.x - dest_x;
    rc.w -= clip.x - dest_x;
    dest_x = clip.x;
  }
  if (dest_y < clip.y) {
    rc.y += clip.y - dest_y;
    rc.h -= clip.y - dest_y;
    dest_y = clip.y;
  }
  if (dest_x+rc.w > clip.x+clip.w) {
    rc.w = clip.x+clip.w-dest_x;
  }
  if (dest_y+rc.h > clip.y+clip.h) {
    rc.h = clip.y+clip.h-dest_y;
  }

  if (rc.isEmpty())
    return;


  auto renderBuf = Editor::getRenderImageBuffer();
  // Generate the rendered image
  if (!renderBuf)
    renderBuf.reset(new doc::ImageBuffer());

  int m_frame = 0;
  base::UniquePtr<Image> rendered(NULL);
  try {
    // Generate a "expose sprite pixels" notification. This is used by
    // tool managers that need to validate this region (copy pixels from
    // the original cel) before it can be used by the RenderEngine.
    {
      gfx::Rect expose = m_proj.remove(rc);

      // If the zoom level is less than 100%, we add extra pixels to
      // the exposed area. Those pixels could be shown in the
      // rendering process depending on each cel position.
      // E.g. when we are drawing in a cel with position < (0,0)
      if (m_proj.scaleX() < 1.0)
        expose.enlargeXW(int(1./m_proj.scaleX()));
      // If the zoom level is more than %100 we add an extra pixel to
      // expose just in case the zoom requires to display it.  Note:
      // this is really necessary to avoid showing invalid destination
      // areas in ToolLoopImpl.
      else if (m_proj.scaleX() > 1.0)
        expose.enlargeXW(1);

      if (m_proj.scaleY() < 1.0)
        expose.enlargeYH(int(1./m_proj.scaleY()));
      else if (m_proj.scaleY() > 1.0)
        expose.enlargeYH(1);

      m_doc->notifyExposeSpritePixels(sprite, gfx::Region(expose));
    }

    // Create a temporary RGBA bitmap to draw all to it
    rendered.reset(Image::create(IMAGE_RGB, rc.w, rc.h, renderBuf));

    m_renderEngine.setRefLayersVisiblity(true);
    //m_renderEngine.setSelectedLayer(m_layer);
    m_renderEngine.setNonactiveLayersOpacity(255);
    m_renderEngine.setProjection(m_proj);
    m_renderEngine.setupBackground(m_doc, rendered->pixelFormat());
    m_renderEngine.disableOnionskin();
    m_renderEngine.setBgType(render::BgType::TRANSPARENT);

    m_renderEngine.renderSprite(
      rendered, sprite, m_frame, gfx::Clip(0, 0, rc));

    m_renderEngine.removeExtraImage();
  }
  catch (const std::exception& e) {
    Console::showException(e);
  }

  if (rendered) {
    // Convert the render to a she::Surface
    static she::Surface* tmp;
    if (!tmp || tmp->width() < rc.w || tmp->height() < rc.h) {
      if (tmp)
        tmp->dispose();

      tmp = she::instance()->createRgbaSurface(rc.w, rc.h);
    }

    if (tmp->nativeHandle()) {
      convert_image_to_surface(rendered, sprite->palette(m_frame),
        tmp, 0, 0, 0, 0, rc.w, rc.h);

      g->drawRgbaSurface(tmp, dest_x, dest_y);
    }
  }
}

} // namespace app
