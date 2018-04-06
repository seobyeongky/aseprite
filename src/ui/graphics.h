// Aseprite UI Library
// Copyright (C) 2001-2017  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef UI_GRAPHICS_H_INCLUDED
#define UI_GRAPHICS_H_INCLUDED
#pragma once

#include "base/disable_copying.h"
#include "base/shared_ptr.h"
#include "base/string.h"
#include "gfx/color.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "gfx/size.h"

#include <string>

namespace gfx {
  class Region;
}

namespace she {
  class DrawTextDelegate;
  class Font;
  class Surface;
}

namespace ui {

  // Class to render a widget in the screen.
  class Graphics {
  public:
    enum class DrawMode {
      Solid,
      Xor,
      Checked,
    };

    Graphics(she::Surface* surface, int dx, int dy);
    ~Graphics();

    int width() const;
    int height() const;

    she::Surface* getInternalSurface() { return m_surface; }
    int getInternalDeltaX() { return m_dx; }
    int getInternalDeltaY() { return m_dy; }

    int getSaveCount() const;
    gfx::Rect getClipBounds() const;
    void saveClip();
    void restoreClip();
    bool clipRect(const gfx::Rect& rc);

    void setDrawMode(DrawMode mode, int param = 0,
                     const gfx::Color a = gfx::ColorNone,
                     const gfx::Color b = gfx::ColorNone);

    gfx::Color getPixel(int x, int y);
    void putPixel(gfx::Color color, int x, int y);

    void drawHLine(gfx::Color color, int x, int y, int w);
    void drawVLine(gfx::Color color, int x, int y, int h);
    void drawLine(gfx::Color color, const gfx::Point& a, const gfx::Point& b);

    void drawRect(gfx::Color color, const gfx::Rect& rc);
    void fillRect(gfx::Color color, const gfx::Rect& rc);
    void fillRegion(gfx::Color color, const gfx::Region& rgn);
    void fillAreaBetweenRects(gfx::Color color,
      const gfx::Rect& outer, const gfx::Rect& inner);

    void drawSurface(she::Surface* surface, int x, int y);
    void drawRgbaSurface(she::Surface* surface, int x, int y);
    void drawRgbaSurface(she::Surface* surface, int srcx, int srcy, int dstx, int dsty, int w, int h);
    void drawColoredRgbaSurface(she::Surface* surface, gfx::Color color, int x, int y);
    void drawColoredRgbaSurface(she::Surface* surface, gfx::Color color, int srcx, int srcy, int dstx, int dsty, int w, int h);

    void blit(she::Surface* src, int srcx, int srcy, int dstx, int dsty, int w, int h);

    // ======================================================================
    // FONT & TEXT
    // ======================================================================

    she::Font* font() { return m_font; }
    void setFont(she::Font* font);

    void drawText(base::utf8_const_iterator it,
                  const base::utf8_const_iterator& end,
                  gfx::Color fg, gfx::Color bg, const gfx::Point& pt,
                  she::DrawTextDelegate* delegate);
    void drawText(const std::string& str, gfx::Color fg, gfx::Color bg, const gfx::Point& pt);
    void drawUIText(const std::string& str, gfx::Color fg, gfx::Color bg, const gfx::Point& pt, const int mnemonic);
    void drawAlignedUIText(const std::string& str, gfx::Color fg, gfx::Color bg, const gfx::Rect& rc, const int align);

    gfx::Size measureUIText(const std::string& str);
    static int measureUITextLength(const std::string& str, she::Font* font);
    gfx::Size fitString(const std::string& str, int maxWidth, int align);

  private:
    gfx::Size doUIStringAlgorithm(const std::string& str, gfx::Color fg, gfx::Color bg, const gfx::Rect& rc, int align, bool draw);
    void dirty(const gfx::Rect& bounds);

    she::Surface* m_surface;
    int m_dx;
    int m_dy;
    gfx::Rect m_clipBounds;
    she::Font* m_font;
    gfx::Rect m_dirtyBounds;
  };

  // Class to draw directly in the screen.
  class ScreenGraphics : public Graphics {
  public:
    ScreenGraphics();
    virtual ~ScreenGraphics();
  };

  // Class to temporary set the Graphics' clip region to the full
  // extend.
  class SetClip {
  public:
    SetClip(Graphics* g)
      : m_graphics(g)
      , m_oldClip(g->getClipBounds())
      , m_oldCount(g->getSaveCount()) {
      if (m_oldCount > 1)
        m_graphics->restoreClip();
    }

    ~SetClip() {
      if (m_oldCount > 1) {
        m_graphics->saveClip();
        m_graphics->clipRect(m_oldClip);
      }
    }

  private:
    Graphics* m_graphics;
    gfx::Rect m_oldClip;
    int m_oldCount;

    DISABLE_COPYING(SetClip);
  };

  // Class to temporary set the Graphics' clip region to a sub-rectangle
  // (in the life-time of the IntersectClip instance).
  class IntersectClip {
  public:
    IntersectClip(Graphics* g, const gfx::Rect& rc)
      : m_graphics(g) {
      m_graphics->saveClip();
      m_notEmpty = m_graphics->clipRect(rc);
    }

    ~IntersectClip() {
      m_graphics->restoreClip();
    }

    operator bool() const { return m_notEmpty; }

  private:
    Graphics* m_graphics;
    bool m_notEmpty;

    DISABLE_COPYING(IntersectClip);
  };

  class CheckedDrawMode {
  public:
    CheckedDrawMode(Graphics* g, int param,
                    const gfx::Color a,
                    const gfx::Color b) : m_graphics(g) {
      m_graphics->setDrawMode(Graphics::DrawMode::Checked, param, a, b);
    }

    ~CheckedDrawMode() {
      m_graphics->setDrawMode(Graphics::DrawMode::Solid);
    }

  private:
    Graphics* m_graphics;

    DISABLE_COPYING(CheckedDrawMode);
  };

  typedef base::SharedPtr<Graphics> GraphicsPtr;

} // namespace ui

#endif
