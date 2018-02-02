// Aseprite
// Copyright (C) 2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/util/wrap_point.h"

#include "app/util/wrap_value.h"

namespace app {

gfx::Point wrap_point(const filters::TiledMode tiledMode,
                      const gfx::Size& spriteSize,
                      const gfx::Point& pt)
{
  gfx::Point out;

  if (int(tiledMode) & int(filters::TiledMode::X_AXIS))
    out.x = wrap_value(pt.x, spriteSize.w);
  else
    out.x = pt.x;

  if (int(tiledMode) & int(filters::TiledMode::Y_AXIS))
    out.y = wrap_value(pt.y, spriteSize.h);
  else
    out.y = pt.y;

  return out;
}

gfx::PointF wrap_pointF(const filters::TiledMode tiledMode,
                        const gfx::Size& spriteSize,
                        const gfx::PointF& pt)
{
  gfx::PointF out;

  if (int(tiledMode) & int(filters::TiledMode::X_AXIS))
    out.x = wrap_value<double>(pt.x, spriteSize.w);
  else
    out.x = pt.x;

  if (int(tiledMode) & int(filters::TiledMode::Y_AXIS))
    out.y = wrap_value<double>(pt.y, spriteSize.h);
  else
    out.y = pt.y;

  return out;
}

} // namespace app
