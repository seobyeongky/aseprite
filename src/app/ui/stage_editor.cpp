// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/stage_editor.h"
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
#include "app/ui/timeline.h"
#include "app/ui/editor/moving_pixels_state.h"
#include "app/ui/editor/pixels_movement.h"
#include "app/ui/editor/play_state.h"
#include "app/ui/editor/scrolling_state.h"
#include "app/ui/editor/standby_state.h"
#include "app/ui/editor/zooming_state.h"
#include "ui/label.h"
#include "doc/frame_tag.h"
#include "app/context.h"
#include "doc/context.h"
#include "app/transaction.h"
#include "app/document_api.h"
#include "doc/handle_anidir.h"
#include "doc/site.h"
#include "app/modules/playables.h"
#include "render/render.h"
#include "render/onionskin_position.h"

const int WIDTH = 150;
const int HEIGHT = 150;

#define DEBUG_MSG App::instance()->mainWindow()->getStageView()->getDbgLabel()->setTextf
#define POSITION_TEXT App::instance()->mainWindow()->getStageView()->getPositionLabel()->setTextf

namespace app {

using namespace ui;
using namespace app::skin;

AppRender StageEditor::m_renderEngine;

StageEditor::StageEditor()
  : m_doublebuf(nullptr)
  , m_doublesur(nullptr)
  , m_bgPal(Palette::createGrayscale())
  , m_docPref("")
  , m_isPlaying(false)
  , m_frame(0)
  , m_isScrolling(false)
  , m_isMoving(false)
  , m_playTimer(10)
  , m_pingPongForward(false)
  , m_loopCount(0)
  , m_zoom(1, 1)
{
  m_playTimer.Tick.connect(&StageEditor::onPlaybackTick, this);
}

StageEditor::~StageEditor()
{
  if (m_doublesur)
    m_doublesur->dispose();
  if (m_doublebuf)
    delete m_doublebuf;
}

void StageEditor::setDocument(Document* doc)
{
  if (m_doc != nullptr) {
    m_doc->remove_observer(this);
  }
  m_doc = doc;
  m_doc->add_observer(this);
}

void StageEditor::getSite(Site* site)
{
  site->document(m_doc);
  if (m_doc) {
    site->sprite(m_doc->sprite());
    if (m_doc->sprite()) {
      site->layer(m_doc->sprite()->firstBrowsableLayer());
      site->frame(m_frame);
    }
  }
}

void StageEditor::onPositionResetButtonClick()
{
  if (m_doc == nullptr || m_doc->sprite() == nullptr)
    return;

  m_previewPos = gfx::Point(0, 0);
  setCurrentFrameRootPosition();
}

void StageEditor::setCurrentFrameRootPosition()
{
  app::Context* context = static_cast<app::Context*>(m_doc->context());
  context->setActiveDocument(m_doc);
  const ContextReader reader(context);
  ContextWriter writer(reader, 500);
  Transaction transaction(writer.context(), "set frame root position", ModifyDocument);
  DocumentApi api = m_doc->getApi(transaction); 
  api.setFrameRootPosition(m_doc->sprite(), m_frame, m_previewPos);
  transaction.commit();
}

void StageEditor::onResize(ui::ResizeEvent& ev)
{
  Widget::onResize(ev);

  m_padding = calcExtraPadding();
}

frame_t StageEditor::frame()
{
  return m_frame;
}

void StageEditor::setFrame(frame_t frame)
{
  if (m_frame == frame)
    return;

  m_frame = frame;
  if (m_doc != nullptr && m_doc->sprite() != nullptr)
  {
    m_previewPos = m_doc->sprite()->frameRootPosition(m_frame);
  }

  updatePositionText();
  invalidate();
}

void StageEditor::play(const bool playOnce,
          const bool playAll)
{
  m_isPlaying = true;
  if (m_doc != nullptr && m_doc->sprite() != nullptr)
  {
    m_nextFrameTime = m_doc->sprite()->frameDuration(m_frame);
    m_curFrameTick = base::current_tick();
    m_loopCount = 0;
    if (!m_playTimer.isRunning())
      m_playTimer.start();
  }
}

void StageEditor::stop()
{
  m_playTimer.stop();
  m_isPlaying = false;
  m_loopCount = 0;
}

bool StageEditor::isPlaying() const
{
  return m_isPlaying;
}

void StageEditor::onFrameRootPositionChanged(DocumentEvent& ev)
{
  m_previewPos = ev.sprite()->frameRootPosition(m_frame);
  updatePositionText();
  invalidate();
}

FrameTag* StageEditor::currentFrameTag(Sprite * sprite)
{
  for (auto frameTag : sprite->frameTags())
  {
    if (frameTag->fromFrame() <= m_frame
      && m_frame <= frameTag->toFrame())
    {
      return frameTag;
    }
  }
  return nullptr;
}

void StageEditor::onPaint(ui::PaintEvent& ev)
{
  Graphics* g = ev.graphics();
  SkinTheme* theme = static_cast<SkinTheme*>(this->theme());

  g->fillRegion(theme->colors.editorFace(), gfx::Region(clientBounds()));

  drawBG(ev);

  if (m_doc == nullptr || m_doc->sprite() == nullptr) {
    return;
  }

  auto sprite = m_doc->sprite();
  auto frameTag = currentFrameTag(sprite);

  if (frameTag == nullptr) {
    return;
  }

  gfx::Point previewPos = playTimePreviewPos(sprite);
  if (previewPos.x < -WIDTH/2 || previewPos.x > WIDTH/2)
    m_loopCount = 0;
  if (previewPos.y < -HEIGHT/2 || previewPos.y > HEIGHT/2)
    m_loopCount = 0;

  previewPos = playTimePreviewPos(sprite);
  gfx::Rect spriteRect(0, 0, sprite->width(), sprite->height());

  // For odd zoom scales minor than 100% we have to add an extra window
  // just to make sure the whole rectangle is drawn.
  if (m_proj.scaleX() < 1.0) spriteRect.w += int(1./m_proj.scaleX());
  if (m_proj.scaleY() < 1.0) spriteRect.h += int(1./m_proj.scaleY());

  drawSprite(g
    , spriteRect
    , previewPos.x
    , previewPos.y
    , sprite);

/*
  gfx::Region outside(clientBounds());
  outside.createSubtraction(outside, gfx::Region(
    gfx::Rect(m_padding.x, m_padding.y, m_proj.applyX(WIDTH), m_proj.applyY(HEIGHT))));
  g->fillRegion(theme->colors.editorFace(), outside);
  */
}

gfx::Point StageEditor::playTimePreviewPos(Sprite* sprite) {
  FrameTag* tag = currentFrameTag(sprite);
  gfx::Point delta = sprite->frameRootPosition(tag->toFrame())
    + sprite->frameRootPosition(tag->fromFrame());
  return gfx::Point(m_previewPos.x + delta.x * m_loopCount
    , m_previewPos.y + delta.y * m_loopCount);
}

bool StageEditor::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kTimerMessage:
      break;

    case kMouseEnterMessage:
      break;

    case kMouseLeaveMessage:
      break;

    case kMouseDownMessage:
      {
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
        if (mouseMsg->middle() || mouseMsg->right()) {
          m_oldMousePos = mouseMsg->position();
          captureMouse();
          m_isScrolling = true;
          return true;
        } else if (mouseMsg->left()) {
          m_oldMousePos = mouseMsg->position();
          captureMouse();
          m_isMoving = true;
        }
      }
      break;

    case kMouseMoveMessage:
      if (m_isScrolling) {
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
        View* view = View::getView(this);
        gfx::Point scroll = view->viewScroll();
        gfx::Point newPos = mouseMsg->position();

        scroll -= newPos - m_oldMousePos;
        m_oldMousePos = newPos;
        view->setViewScroll(scroll);
        //DEBUG_MSG("scroll x(%d) y(%d)", scroll.x, scroll.y);
        return true;
      } else if (m_isMoving) {
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
        gfx::Point delta = mouseMsg->position() - m_oldMousePos;
        m_oldMousePos = mouseMsg->position();
        if (m_doc != nullptr && m_doc->sprite() != nullptr)
        {
          m_previewPos += delta;

          updatePositionText();
          invalidate();
          return true;
        }
      }
      break;

    case kMouseUpMessage:
      if (m_isScrolling)
      {
        releaseMouse();
        m_isScrolling = false;
        return true;
      } else if (m_isMoving) {
        if (m_doc != nullptr && m_doc->sprite() != nullptr) {
          setCurrentFrameRootPosition();
        }

        releaseMouse();
        m_isMoving = false;
        return true;
      }
      break;

    case kDoubleClickMessage:
      break;

    case kTouchMagnifyMessage:
      break;

    case kKeyDownMessage:
      break;

    case kKeyUpMessage:
      break;

    case kFocusLeaveMessage:
      break;

    case kMouseWheelMessage:
      {
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg); 
        bool dirty = false;
        if (mouseMsg->wheelDelta().y < 0) {
          m_zoom.in();
          dirty = true;
        }
        else if (mouseMsg->wheelDelta().y > 0) {
          m_zoom.out();
          dirty = true;
        }
        if (dirty) {
          m_proj.setZoom(m_zoom);
          View::getView(this)->updateView();
          //invalidate();
        }
      }
      break;

    case kSetCursorMessage:
      break;
  }

  return Widget::onProcessMessage(msg);
}

void StageEditor::onSizeHint(SizeHintEvent& ev)
{
  gfx::Size sz(0, 0);
  if (m_doc != nullptr && m_doc->sprite() != nullptr)
  {
    sz.w = 2 * calcExtraPadding().x + m_proj.applyX(WIDTH);
    sz.h = 2 * calcExtraPadding().y + m_proj.applyY(HEIGHT);
  }

  ev.setSizeHint(sz);
}


void StageEditor::drawBG(ui::PaintEvent& ev)
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

  if (m_doublebuf)
    delete m_doublebuf;

  auto renderBuf = Editor::getRenderImageBuffer();
  // Generate the rendered image
  if (!renderBuf)
    renderBuf.reset(new doc::ImageBuffer());

  base::UniquePtr<Image> bgBuf(NULL);
  bgBuf.reset(Image::create(IMAGE_RGB, m_proj.applyX(WIDTH), m_proj.applyY(HEIGHT), renderBuf));

  m_renderEngine.setBgType(bgType);
  m_renderEngine.setBgZoom(m_docPref.bg.zoom());
  m_renderEngine.setBgColor1(color_utils::color_for_image(m_docPref.bg.color1(), bgBuf->pixelFormat()));
  m_renderEngine.setBgColor2(color_utils::color_for_image(m_docPref.bg.color2(), bgBuf->pixelFormat()));
  m_renderEngine.setBgCheckedSize(tile);

  //m_renderEngine.setupBackground(m_doc, m_doublebuf->pixelFormat());
  m_renderEngine.renderBackground(bgBuf,
    gfx::Clip(0, 0, 0, 0,
      bgBuf->width(), bgBuf->height()));

  if (m_doublesur == nullptr
    || m_doublesur->width() != bgBuf->width()
    || m_doublesur->height() != bgBuf->height()) {
    if (m_doublesur)
      m_doublesur->dispose();

    m_doublesur = she::instance()->createSurface(m_proj.applyX(WIDTH), m_proj.applyY(HEIGHT));
  }

  convert_image_to_surface(bgBuf, m_bgPal,
    m_doublesur, 0, 0, 0, 0, bgBuf->width(), bgBuf->height());
  g->blit(m_doublesur, 0, 0, m_padding.x, m_padding.y
    , m_proj.applyX(m_doublesur->width())
    , m_proj.applyY(m_doublesur->height())); 
}

void StageEditor::drawSprite(ui::Graphics* g
  , const gfx::Rect& spriteRectToDraw
  , int dx
  , int dy
  , Sprite * sprite)
{
  // Clip from sprite and apply zoom
  gfx::Rect rc = sprite->bounds().createIntersection(spriteRectToDraw);
  rc = m_proj.apply(rc);

  int dest_x = dx + m_padding.x + rc.x + m_proj.applyX(WIDTH)/2 - spriteRectToDraw.w/2;
  int dest_y = dy + m_padding.y + rc.y + m_proj.applyY(HEIGHT)/2 - spriteRectToDraw.h/2;

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

  FrameTag* tag = currentFrameTag(sprite);
  auto renderBuf = Editor::getRenderImageBuffer();
  // Generate the rendered image
  if (!renderBuf)
    renderBuf.reset(new doc::ImageBuffer());

  base::UniquePtr<Image> rendered(NULL);
  try {
    // Create a temporary RGBA bitmap to draw all to it
    rendered.reset(Image::create(IMAGE_RGB, rc.w, rc.h, renderBuf));

    m_renderEngine.setRefLayersVisiblity(true);
    //m_renderEngine.setSelectedLayer(m_layer);
    m_renderEngine.setNonactiveLayersOpacity(255);
    m_renderEngine.setProjection(m_proj);
    m_renderEngine.setupBackground(m_doc, rendered->pixelFormat());
    if (!m_isPlaying) {
      render::OnionskinOptions opts(render::OnionskinType::MERGE);
      opts.position(render::OnionskinPosition::BEHIND);
      opts.prevFrames(m_frame - tag->fromFrame());
      opts.nextFrames(tag->toFrame() - m_frame);
      opts.opacityBase(100);
      opts.opacityStep(100);
      opts.layer(nullptr);
      opts.loopTag(tag);
      opts.applyRootPosition(true);
      m_renderEngine.setOnionskin(opts);
    }
    else {
      m_renderEngine.disableOnionskin();
    }
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

void StageEditor::updatePositionText()
{
  if (m_doc == nullptr || m_doc->sprite() == nullptr)
  {
    POSITION_TEXT("");
    return;
  }

  POSITION_TEXT("frame %d root position : %d %d"
    , m_frame, m_previewPos.x, m_previewPos.y);
}

void StageEditor::onPlaybackTick()
{
  if (m_nextFrameTime < 0)
    return;

  if (m_doc == nullptr || m_doc->sprite() == nullptr)
  {
    return;
  }

  m_nextFrameTime -= (base::current_tick() - m_curFrameTick);
  Sprite* sprite = m_doc->sprite();
  FrameTag* tag = currentFrameTag(sprite);

  while (m_nextFrameTime <= 0) {
    bool atEnd = false;
    if (tag) {
      switch (tag->aniDir()) {
        case AniDir::FORWARD:
          atEnd = (m_frame == tag->toFrame());
          break;
        case AniDir::REVERSE:
          atEnd = (m_frame == tag->fromFrame());
          break;
        case AniDir::PING_PONG:
          atEnd = (!m_pingPongForward &&
                   m_frame == tag->fromFrame());
          break;
      }
    }
    else {
      atEnd = (m_frame == sprite->lastFrame());
    }
    if (atEnd) {
      m_loopCount++;
    }

    setFrame(calculate_next_frame(
      sprite, m_frame, frame_t(1), tag,
      m_pingPongForward));

    m_nextFrameTime += sprite->frameDuration(m_frame);
  }

  m_curFrameTick = base::current_tick();
}

gfx::Point StageEditor::calcExtraPadding()
{
  View* view = View::getView(this);
  if (view) {
    gfx::Rect vp = view->viewportBounds();
    return gfx::Point(
      std::max<int>(vp.w/2, vp.w - m_proj.applyX(WIDTH)),
      std::max<int>(vp.h/2, vp.h - m_proj.applyY(HEIGHT)));
  }
  else
    return gfx::Point(0, 0);
}

} // namespace app
