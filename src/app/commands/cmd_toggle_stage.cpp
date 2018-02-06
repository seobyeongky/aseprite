#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/context.h"
#include "app/ui/main_window.h"
#include "app/ui/stage_view.h"
#include "app/ui/workspace.h"

namespace app {

class ToggleStageCommand : public Command {
public:
  ToggleStageCommand();
  Command* clone() const override { return new ToggleStageCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  bool onChecked(Context* context) override;
  void onExecute(Context* context) override;
};

ToggleStageCommand::ToggleStageCommand()
  : Command("ToggleStage",  
            CmdUIOnlyFlag)
{
}

bool ToggleStageCommand::onEnabled(Context* context)
{
  return true;
}

bool ToggleStageCommand::onChecked(Context* context)
{
  MainWindow* mainWin = App::instance()->mainWindow();
  if (!mainWin)
    return false;

  StageView* stageView = mainWin->getStageView();
  return stageView && stageView->isVisible();
}

void ToggleStageCommand::onExecute(Context* context)
{
  auto stageView = App::instance()->mainWindow()->getStageView();
  if (stageView && stageView->isVisible())
  {
    App::instance()->workspace()->closeView(stageView, false);
  }
  else
  {
    App::instance()->mainWindow()->showStage();
  }
}

Command* CommandFactory::createToggleStageCommand()
{
  return new ToggleStageCommand;
}

} // namespace app
