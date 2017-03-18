// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_STAGE_EDITOR_H_INCLUDED
#define APP_UI_STAGE_EDITOR_H_INCLUDED
#pragma once

#include "app/app_render.h"
#include "app/ui/tabs.h"
#include "app/ui/workspace_view.h"
#include "app/ui/editor/editor.h"
#include "app/ui/playable.h"
#include "ui/box.h"
#include "app/pref/preferences.h"
#include "app/ui/editor/editor_state.h"
#include "doc/document_observer.h"

namespace ui {
  class View;
  class Label;
}
namespace doc {
  class Palette;
  class FrameTag;
  class Site;
}

namespace render {
}

namespace app {
  using namespace doc;

  class StageEditor : public ui::Widget
                    , public Playable
                    , public DocumentObserver
  {
  public:
    StageEditor();
    ~StageEditor();

    void setDocument(Document* doc);
    Document* getDoc() const {return m_doc;}
    void getSite(Site* site);

    // Playable implementation
    frame_t frame() override;
    void setFrame(frame_t frame) override;
    void play(const bool playOnce,
                      const bool playAll) override;
    void stop() override;
    bool isPlaying() const override;

    // DocumentObserver implementation
    void onFrameRootPositionChanged(DocumentEvent& ev);

  protected:
    void onPaint(ui::PaintEvent& ev) override;
    void onResize(ui::ResizeEvent& ev) override;
    bool onProcessMessage(ui::Message* msg) override;
    void onSizeHint(ui::SizeHintEvent& ev) override;

  private:
    static AppRender m_renderEngine;

    Document* m_doc;
    // Extra space around the sprite.
    gfx::Point m_padding;
    render::Projection m_proj;    // Zoom/pixel ratio in the editor
    Image* m_doublebuf;
    she::Surface* m_doublesur;

    gfx::Point m_pos;
    gfx::Point m_oldMousePos;
    gfx::Point m_delta;
    gfx::Point m_previewPos;

    Palette* m_bgPal;
    DocumentPreferences m_docPref;

    bool m_isPlaying;
    frame_t m_frame;
    bool m_isScrolling;
    bool m_isMoving;

    ui::Timer m_playTimer;
    // Number of milliseconds to go to the next frame if m_playTimer
    // is activated.
    double m_nextFrameTime;
    base::tick_t m_curFrameTick;
    bool m_pingPongForward;

    void drawBG(ui::PaintEvent& ev);
    void drawSprite(ui::Graphics* g
      , const gfx::Rect& spriteRectToDraw
      , int dx
      , int dy
      , Sprite* sprite
      , frame_t frame);

    FrameTag* currentFrameTag(Sprite * sprite);
    void updatePositionText();
    void onPlaybackTick();
  };

} // namespace app

#endif
