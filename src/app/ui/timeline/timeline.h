// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_TIMELINE_TIMELINE_H_INCLUDED
#define APP_UI_TIMELINE_TIMELINE_H_INCLUDED
#pragma once

#include "app/document_range.h"
#include "app/loop_tag.h"
#include "app/pref/preferences.h"
#include "app/ui/editor/editor_observer.h"
#include "app/ui/input_chain_element.h"
#include "app/ui/playable.h"
#include "app/ui/timeline/ani_controls.h"
#include "doc/document_observer.h"
#include "doc/documents_observer.h"
#include "doc/frame.h"
#include "doc/selected_frames.h"
#include "doc/selected_layers.h"
#include "doc/sprite.h"
#include "obs/connection.h"
#include "ui/scroll_bar.h"
#include "ui/timer.h"
#include "ui/widget.h"

#include <vector>

namespace doc {
  class Cel;
  class Layer;
  class LayerImage;
  class Sprite;
}

namespace ui {
  class Graphics;
}

namespace app {

  namespace skin {
    class SkinTheme;
  }

  using namespace doc;

  class CommandExecutionEvent;
  class ConfigureTimelinePopup;
  class Context;
  class Document;
  class Editor;
  class StageEditor;

  class Timeline : public ui::Widget
                 , public ui::ScrollableViewDelegate
                 , public doc::DocumentsObserver
                 , public doc::DocumentObserver
                 , public app::EditorObserver
                 , public app::InputChainElement
                 , public app::FrameTagProvider {
  public:
    typedef DocumentRange Range;

    enum State {
      STATE_STANDBY,
      STATE_SCROLLING,
      STATE_SELECTING_LAYERS,
      STATE_SELECTING_FRAMES,
      STATE_SELECTING_CELS,
      STATE_MOVING_SEPARATOR,
      STATE_MOVING_RANGE,
      STATE_MOVING_ONIONSKIN_RANGE_LEFT,
      STATE_MOVING_ONIONSKIN_RANGE_RIGHT
    };

    enum DropOp { kMove, kCopy };

    Timeline();
    ~Timeline();

    void updateUsingEditor(Editor* editor);
    void updateUsingStageEditor(StageEditor* stageEditor);

    Sprite* sprite() { return m_sprite; }
    Layer* getLayer() { return m_layer; }
    frame_t getFrame() { return m_frame; }

    State getState() const { return m_state; }
    bool isMovingCel() const;

    Range range() const { return m_range; }
    const SelectedLayers& selectedLayers() const { return m_range.selectedLayers(); }
    const SelectedFrames& selectedFrames() const { return m_range.selectedFrames(); }

    void prepareToMoveRange();
    void moveRange(Range& range);

    void activateClipboardRange();

    // Drag-and-drop operations. These actions are used by commands
    // called from popup menus.
    void dropRange(DropOp op);

    // FrameTagProvider impl
    // Returns the active frame tag depending on the timeline status
    // E.g. if other frame tags are collapsed, the focused band has
    // priority and tags in other bands are ignored.
    FrameTag* getFrameTagByFrame(const frame_t frame) override;

    // ScrollableViewDelegate impl
    gfx::Size visibleSize() const override;
    gfx::Point viewScroll() const override;
    void setViewScroll(const gfx::Point& pt) override;

///???????????????
    void manualUpdateAniControls();
///   
    void lockRange();
    void unlockRange();

  protected:
    bool onProcessMessage(ui::Message* msg) override;
    void onInitTheme(ui::InitThemeEvent& ev) override;
    void onInvalidateRegion(const gfx::Region& region) override;
    void onSizeHint(ui::SizeHintEvent& ev) override;
    void onResize(ui::ResizeEvent& ev) override;
    void onPaint(ui::PaintEvent& ev) override;

    // DocumentObserver impl.
    void onGeneralUpdate(DocumentEvent& ev) override;
    void onAddLayer(doc::DocumentEvent& ev) override;
    void onAfterRemoveLayer(doc::DocumentEvent& ev) override;
    void onAddFrame(doc::DocumentEvent& ev) override;
    void onRemoveFrame(doc::DocumentEvent& ev) override;
    void onSelectionChanged(doc::DocumentEvent& ev) override;
    void onLayerNameChange(doc::DocumentEvent& ev) override;
    void onAddFrameTag(DocumentEvent& ev) override;
    void onRemoveFrameTag(DocumentEvent& ev) override;

    // app::Context slots.
    void onAfterCommandExecution(CommandExecutionEvent& ev);

    // DocumentsObserver impl.
    void onRemoveDocument(doc::Document* document) override;

    // EditorObserver impl.
    void onStateChanged(Editor* editor) override;
    void onAfterFrameChanged(Editor* editor) override;
    void onAfterLayerChanged(Editor* editor) override;
    void onDestroyEditor(Editor* editor) override;

    // InputChainElement impl
    void onNewInputPriority(InputChainElement* element) override;
    bool onCanCut(Context* ctx) override;
    bool onCanCopy(Context* ctx) override;
    bool onCanPaste(Context* ctx) override;
    bool onCanClear(Context* ctx) override;
    bool onCut(Context* ctx) override;
    bool onCopy(Context* ctx) override;
    bool onPaste(Context* ctx) override;
    bool onClear(Context* ctx) override;
    void onCancel(Context* ctx) override;

  private:
    struct DrawCelData;

    struct Hit {
      int part;
      layer_t layer;
      frame_t frame;
      ObjectId frameTag;
      bool veryBottom;
      int band;

      Hit(int part = 0,
          layer_t layer = -1,
          frame_t frame = 0,
          ObjectId frameTag = NullId,
          int band = -1);
      bool operator!=(const Hit& other) const;
      FrameTag* getFrameTag() const;
    };

    struct DropTarget {
      enum HHit {
        HNone,
        Before,
        After
      };
      enum VHit {
        VNone,
        Bottom,
        Top,
        FirstChild,
        VeryBottom
      };

      DropTarget();

      HHit hhit;
      VHit vhit;
      Layer* layer;
      ObjectId layerId;
      frame_t frame;
      int xpos, ypos;
    };

    struct Row {
      Row();
      Row(Layer* layer,
          const int level,
          const LayerFlags inheritedFlags);

      Layer* layer() const { return m_layer; }
      int level() const { return m_level; }

      bool parentVisible() const;
      bool parentEditable() const;

    private:
      Layer* m_layer;
      int m_level;
      LayerFlags m_inheritedFlags;
    };

    bool selectedLayersBounds(const SelectedLayers& layers,
                              layer_t* first, layer_t* last) const;

    void setLayer(Layer* layer);
    void setFrame(frame_t frame, bool byUser);
    bool allLayersVisible();
    bool allLayersInvisible();
    bool allLayersLocked();
    bool allLayersUnlocked();
    bool allLayersContinuous();
    bool allLayersDiscontinuous();
    void detachDocument();
    void setCursor(ui::Message* msg, const Hit& hit);
    void getDrawableLayers(ui::Graphics* g, layer_t* firstLayer, layer_t* lastLayer);
    void getDrawableFrames(ui::Graphics* g, frame_t* firstFrame, frame_t* lastFrame);
    void drawPart(ui::Graphics* g, const gfx::Rect& bounds,
                  const std::string* text,
                  ui::Style* style,
                  const bool is_active = false,
                  const bool is_hover = false,
                  const bool is_clicked = false,
                  const bool is_disabled = false);
    void drawTop(ui::Graphics* g);
    void drawHeader(ui::Graphics* g);
    void drawHeaderFrame(ui::Graphics* g, frame_t frame);
    void drawLayer(ui::Graphics* g, layer_t layerIdx);
    void drawCel(ui::Graphics* g, layer_t layerIdx, frame_t frame, Cel* cel, DrawCelData* data);
    void drawCelLinkDecorators(ui::Graphics* g, const gfx::Rect& bounds,
                               Cel* cel, frame_t frame, bool is_active, bool is_hover,
                               DrawCelData* data);
    void drawFrameTags(ui::Graphics* g);
    void drawRangeOutline(ui::Graphics* g);
    void drawPaddings(ui::Graphics* g);
    bool drawPart(ui::Graphics* g, int part, layer_t layer, frame_t frame);
    void drawClipboardRange(ui::Graphics* g);
    gfx::Rect getLayerHeadersBounds() const;
    gfx::Rect getFrameHeadersBounds() const;
    gfx::Rect getOnionskinFramesBounds() const;
    gfx::Rect getCelsBounds() const;
    gfx::Rect getPartBounds(const Hit& hit) const;
    gfx::Rect getRangeBounds(const Range& range) const;
    gfx::Rect getRangeClipBounds(const Range& range) const;
    void invalidateHit(const Hit& hit);
    void invalidateLayer(const Layer* layer);
    void invalidateFrame(const frame_t frame);
    void invalidateRange();
    void regenerateRows();
    void regenerateTagBands();
    int visibleTagBands() const;
    void updateScrollBars();
    void updateByMousePos(ui::Message* msg, const gfx::Point& mousePos);
    Hit hitTest(ui::Message* msg, const gfx::Point& mousePos);
    Hit hitTestCel(const gfx::Point& mousePos);
    void setHot(const Hit& hit);
    void showCel(layer_t layer, frame_t frame);
    void showCurrentCel();
    void focusTagBand(int band);
    void cleanClk();
    gfx::Size getScrollableSize() const;
    gfx::Point getMaxScrollablePos() const;
    layer_t getLayerIndex(const Layer* layer) const;
    bool isLayerActive(const layer_t layerIdx) const;
    bool isFrameActive(const frame_t frame) const;
    void updateStatusBar(ui::Message* msg);
    void updateDropRange(const gfx::Point& pt);
    void clearClipboardRange();
    void clearAndInvalidateRange();

    // The layer of the bottom (e.g. Background layer)
    layer_t firstLayer() const { return 0; }
    // The layer of the top.
    layer_t lastLayer() const { return m_rows.size()-1; }

    frame_t firstFrame() const { return frame_t(0); }
    frame_t lastFrame() const { return m_sprite->lastFrame(); }

    bool validLayer(layer_t layer) const { return layer >= firstLayer() && layer <= lastLayer(); }
    bool validFrame(frame_t frame) const { return frame >= firstFrame() && frame <= lastFrame(); }

    int topHeight() const;

    DocumentPreferences& docPref() const;

    // Theme/dimensions
    skin::SkinTheme* skinTheme() const;
    gfx::Size celBoxSize() const;
    int headerBoxWidth() const;
    int headerBoxHeight() const;
    int layerBoxHeight() const;
    int frameBoxWidth() const;
    int outlineWidth() const;
    int oneTagHeight() const;
    int calcTagVisibleToFrame(FrameTag* frameTag) const;

    void updateCelOverlayBounds(const Hit& hit);
    void drawCelOverlay(ui::Graphics* g);
    void onThumbnailsPrefChange();
    void setZoom(const double zoom);
    void setZoomAndUpdate(const double zoom,
                          const bool updatePref);

    double zoom() const;

    ui::ScrollBar m_hbar;
    ui::ScrollBar m_vbar;
    gfx::Rect m_viewportArea;
    double m_zoom;
    Context* m_context;
    Editor* m_editor;
    Playable* m_playable;
    Document* m_document;
    Sprite* m_sprite;
    Layer* m_layer;
    frame_t m_frame;
    int m_rangeLocks;
    Range m_range;
    Range m_startRange;
    Range m_dropRange;
    State m_state;

    // Data used to display each row in the timeline
    std::vector<Row> m_rows;

    // Data used to display frame tags
    int m_tagBands;
    int m_tagFocusBand;
    std::map<FrameTag*, int> m_tagBand;

    int m_separator_x;
    int m_separator_w;
    int m_origFrames;
    Hit m_hot;       // The 'hot' part is where the mouse is on top of
    DropTarget m_dropTarget;
    Hit m_clk; // The 'clk' part is where the mouse's button was pressed (maybe for a drag & drop operation)
    // Absolute mouse positions for scrolling.
    gfx::Point m_oldPos;
    // Configure timeline
    ConfigureTimelinePopup* m_confPopup;
    obs::scoped_connection m_ctxConn;
    obs::connection m_firstFrameConn;

    // Marching ants stuff to show the range in the clipboard.
    // TODO merge this with the marching ants of the sprite editor (ui::Editor)
    ui::Timer m_clipboard_timer;
    int m_offset_count;
    bool m_redrawMarchingAntsOnly;

    bool m_scroll;   // True if the drag-and-drop operation is a scroll operation.
    bool m_copy;     // True if the drag-and-drop operation is a copy.
    bool m_fromTimeline;

    AniControls m_aniControls;

    // Data used for thumbnails.
    bool m_thumbnailsOverlayVisible;
    gfx::Rect m_thumbnailsOverlayInner;
    gfx::Rect m_thumbnailsOverlayOuter;
    Hit m_thumbnailsOverlayHit;
    gfx::Point m_thumbnailsOverlayDirection;
    obs::connection m_thumbnailsPrefConn;

    // Temporal data used to move the range.
    struct MoveRange {
      layer_t activeRelativeLayer;
      frame_t activeRelativeFrame;
    } m_moveRangeData;
  };

  class LockTimelineRange {
  public:
    LockTimelineRange(Timeline* timeline)
      : m_timeline(timeline) {
      m_timeline->lockRange();
    }
    ~LockTimelineRange() {
      m_timeline->unlockRange();
    }
  private:
    Timeline* m_timeline;
  };

} // namespace app

#endif
