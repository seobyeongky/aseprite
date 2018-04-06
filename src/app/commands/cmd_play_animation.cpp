// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/command.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/modules/playables.h"
#include "app/pref/preferences.h"
#include "app/ui/editor/editor.h"

namespace app {

using namespace ui;

class PlayAnimationCommand : public Command {
public:
  PlayAnimationCommand();
  Command* clone() const override { return new PlayAnimationCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

PlayAnimationCommand::PlayAnimationCommand()
  : Command(CommandId::PlayAnimation(), CmdUIOnlyFlag)
{
}

bool PlayAnimationCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::HasActiveSprite);
}

void PlayAnimationCommand::onExecute(Context* context)
{
  // Do not play one-frame images
  {
    ContextReader writer(context);
    Sprite* sprite(writer.sprite());
    if (!sprite || sprite->totalFrames() < 2)
      return;
  }

  ASSERT(current_playable);
  if (!current_playable)
    return;

  if (current_playable->isPlaying())
    current_playable->stop();
  else
    current_playable->play(Preferences::instance().editor.playOnce(),
                         Preferences::instance().editor.playAll());
}

Command* CommandFactory::createPlayAnimationCommand()
{
  return new PlayAnimationCommand;
}

} // namespace app
