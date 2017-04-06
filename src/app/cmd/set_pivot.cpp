#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cmd/set_pivot.h"

#include "doc/document.h"
#include "doc/document_event.h"
#include "doc/document_observer.h"
#include "doc/sprite.h"

namespace app {
namespace cmd {

using namespace doc;

SetPivot::SetPivot(Sprite* sprite, gfx::PointF pivot)
  : WithSprite(sprite)
  , m_oldPivot(sprite->pivot())
  , m_newPivot(pivot)
{
}

void SetPivot::onExecute()
{
  Sprite* spr = sprite();
  spr->setPivot(m_newPivot);
  spr->incrementVersion();
}

void SetPivot::onUndo()
{
  Sprite* spr = sprite();
  spr->setPivot(m_oldPivot);
  spr->incrementVersion();
}

void SetPivot::onFireNotifications()
{
  Sprite* sprite = this->sprite();
  DocumentEvent ev(sprite->document());
  ev.sprite(sprite);
}

} // namespace cmd
} // namespace app
