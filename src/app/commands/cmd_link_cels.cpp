// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/cmd/copy_cel.h"
#include "app/commands/command.h"
#include "app/context_access.h"
#include "app/modules/gui.h"
#include "app/transaction.h"
#include "app/ui/status_bar.h"
#include "doc/cel.h"
#include "doc/layer.h"
#include "doc/sprite.h"

namespace app {

class LinkCelsCommand : public Command {
public:
  LinkCelsCommand();
  Command* clone() const override { return new LinkCelsCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

LinkCelsCommand::LinkCelsCommand()
  : Command(CommandId::LinkCels(), CmdRecordableFlag)
{
}

bool LinkCelsCommand::onEnabled(Context* context)
{
  if (context->checkFlags(ContextFlags::ActiveDocumentIsWritable)) {
    auto site = context->activeSite();
    return (site.inTimeline() &&
            site.selectedFrames().size() > 1);
  }
  else
    return false;
}

void LinkCelsCommand::onExecute(Context* context)
{
  ContextWriter writer(context);
  Document* document(writer.document());
  bool nonEditableLayers = false;
  {
    auto site = context->activeSite();
    if (!site.inTimeline())
      return;

    Transaction transaction(writer.context(), friendlyName());

    for (Layer* layer : site.selectedLayers()) {
      if (!layer->isImage())
        continue;

      if (!layer->isEditableHierarchy()) {
        nonEditableLayers = true;
        continue;
      }

      LayerImage* layerImage = static_cast<LayerImage*>(layer);

      for (auto it=site.selectedFrames().begin(), end=site.selectedFrames().end();
           it != end; ++it) {
        frame_t frame = *it;
        Cel* cel = layerImage->cel(frame);
        if (cel) {
          for (++it; it != end; ++it) {
            transaction.execute(
              new cmd::CopyCel(
                layerImage, cel->frame(),
                layerImage, *it,
                true));         // true = force links
          }
          break;
        }
      }
    }

    transaction.commit();
  }

  if (nonEditableLayers)
    StatusBar::instance()->showTip(1000,
      "There are locked layers");

  update_screen_for_document(document);
}

Command* CommandFactory::createLinkCelsCommand()
{
  return new LinkCelsCommand;
}

} // namespace app
