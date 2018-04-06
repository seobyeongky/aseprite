// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/command.h"
#include "app/context_access.h"
#include "app/document_api.h"
#include "app/modules/gui.h"
#include "app/transaction.h"
#include "app/ui/color_bar.h"
#include "doc/layer.h"
#include "doc/sprite.h"

namespace app {

class BackgroundFromLayerCommand : public Command {
public:
  BackgroundFromLayerCommand();
  Command* clone() const override { return new BackgroundFromLayerCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

BackgroundFromLayerCommand::BackgroundFromLayerCommand()
  : Command(CommandId::BackgroundFromLayer(), CmdRecordableFlag)
{
}

bool BackgroundFromLayerCommand::onEnabled(Context* context)
{
  return
    context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                        ContextFlags::ActiveLayerIsVisible |
                        ContextFlags::ActiveLayerIsEditable |
                        ContextFlags::ActiveLayerIsImage) &&
    // Doesn't have a background layer
    !context->checkFlags(ContextFlags::HasBackgroundLayer) &&
    // Isn't a reference layer
    !context->checkFlags(ContextFlags::ActiveLayerIsReference);
}

void BackgroundFromLayerCommand::onExecute(Context* context)
{
  ContextWriter writer(context);
  Document* document(writer.document());

  {
    Transaction transaction(writer.context(), "Background from Layer");
    document->getApi(transaction).backgroundFromLayer(
      static_cast<LayerImage*>(writer.layer()));
    transaction.commit();
  }

  update_screen_for_document(document);
}

Command* CommandFactory::createBackgroundFromLayerCommand()
{
  return new BackgroundFromLayerCommand;
}

} // namespace app
