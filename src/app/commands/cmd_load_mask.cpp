// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cmd/set_mask.h"
#include "app/commands/command.h"
#include "app/commands/params.h"
#include "app/context_access.h"
#include "app/file_selector.h"
#include "app/i18n/strings.h"
#include "app/modules/gui.h"
#include "app/transaction.h"
#include "app/util/msk_file.h"
#include "doc/mask.h"
#include "doc/sprite.h"
#include "fmt/format.h"
#include "ui/alert.h"

namespace app {

class LoadMaskCommand : public Command {
  std::string m_filename;

public:
  LoadMaskCommand();
  Command* clone() const override { return new LoadMaskCommand(*this); }

protected:
  void onLoadParams(const Params& params) override;
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

LoadMaskCommand::LoadMaskCommand()
  : Command(CommandId::LoadMask(), CmdRecordableFlag)
{
  m_filename = "";
}

void LoadMaskCommand::onLoadParams(const Params& params)
{
  m_filename = params.get("filename");
}

bool LoadMaskCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable);
}

void LoadMaskCommand::onExecute(Context* context)
{
  const ContextReader reader(context);

  if (context->isUIAvailable()) {
    base::paths exts = { "msk" };
    base::paths selectedFilename;
    if (!app::show_file_selector(
          "Load .msk File", m_filename, exts,
          FileSelectorType::Open, selectedFilename))
      return;

    m_filename = selectedFilename.front();
  }

  base::UniquePtr<Mask> mask(load_msk_file(m_filename.c_str()));
  if (!mask) {
    ui::Alert::show(fmt::format(Strings::alerts_error_loading_file(), m_filename));
    return;
  }

  {
    ContextWriter writer(reader);
    Document* document = writer.document();
    Transaction transaction(writer.context(), "Mask Load", DoesntModifyDocument);
    transaction.execute(new cmd::SetMask(document, mask));
    transaction.commit();

    document->generateMaskBoundaries();
    update_screen_for_document(document);
  }
}

Command* CommandFactory::createLoadMaskCommand()
{
  return new LoadMaskCommand;
}

} // namespace app
