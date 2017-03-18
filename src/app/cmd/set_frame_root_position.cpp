// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cmd/set_frame_root_position.h"

#include "app/document.h"
#include "doc/document_event.h"
#include "doc/sprite.h"

namespace app {
namespace cmd {

SetFrameRootPosition::SetFrameRootPosition(Sprite* sprite, frame_t frame
  , const gfx::Point & p)
  : WithSprite(sprite)
  , m_frame(frame)
  , m_oldPosition(sprite->frameRootPosition(frame))
  , m_newPosition(p)
{
}

void SetFrameRootPosition::onExecute()
{
  sprite()->setFrameRootPosition(m_frame, m_newPosition);
  sprite()->incrementVersion();
}

void SetFrameRootPosition::onUndo()
{
  sprite()->setFrameRootPosition(m_frame, m_oldPosition);
  sprite()->incrementVersion();
}

void SetFrameRootPosition::onFireNotifications()
{
  Sprite* sprite = this->sprite();
  doc::Document* doc = sprite->document();
  DocumentEvent ev(doc);
  ev.sprite(sprite);
  ev.frame(m_frame);
  doc->notify_observers<DocumentEvent&>(&DocumentObserver::onFrameRootPositionChanged, ev);
}

} // namespace cmd
} // namespace app
