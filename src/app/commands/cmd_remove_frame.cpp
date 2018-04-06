// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/context_access.h"
#include "app/document_api.h"
#include "app/modules/gui.h"
#include "app/transaction.h"
#include "doc/sprite.h"
#include "ui/ui.h"

namespace app {

class RemoveFrameCommand : public Command {
public:
  RemoveFrameCommand();
  Command* clone() const override { return new RemoveFrameCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

RemoveFrameCommand::RemoveFrameCommand()
  : Command(CommandId::RemoveFrame(), CmdRecordableFlag)
{
}

bool RemoveFrameCommand::onEnabled(Context* context)
{
  ContextWriter writer(context);
  Sprite* sprite(writer.sprite());
  return
    sprite &&
    sprite->totalFrames() > 1;
}

void RemoveFrameCommand::onExecute(Context* context)
{
  ContextWriter writer(context);
  Document* document(writer.document());
  Sprite* sprite(writer.sprite());
  {
    Transaction transaction(writer.context(), "Remove Frame");
    DocumentApi api = document->getApi(transaction);
    const Site* site = writer.site();
    if (site->inTimeline() &&
        !site->selectedFrames().empty()) {
      for (frame_t frame : site->selectedFrames().reversed()) {
        api.removeFrame(sprite, frame);
      }
    }
    else {
      api.removeFrame(sprite, writer.frame());
    }

    transaction.commit();
  }
  update_screen_for_document(document);
}

Command* CommandFactory::createRemoveFrameCommand()
{
  return new RemoveFrameCommand;
}

} // namespace app
