// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/main_window.h"

#include "app/app.h"
#include "app/app_menus.h"
#include "app/commands/commands.h"
#include "app/i18n/strings.h"
#include "app/ini_file.h"
#include "app/modules/editors.h"
#include "app/notification_delegate.h"
#include "app/pref/preferences.h"
#include "app/ui/browser_view.h"
#include "app/ui/color_bar.h"
#include "app/ui/context_bar.h"
#include "app/ui/document_view.h"
#include "app/ui/editor/editor.h"
#include "app/ui/editor/editor_view.h"
#include "app/ui/home_view.h"
#include "app/ui/main_menu_bar.h"
#include "app/ui/notifications.h"
#include "app/ui/preview_editor.h"
#include "app/ui/stage_view.h"
#include "app/ui/skin/skin_property.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/status_bar.h"
#include "app/ui/timeline/timeline.h"
#include "app/ui/toolbar.h"
#include "app/ui/workspace.h"
#include "app/ui/workspace_tabs.h"
#include "app/ui_context.h"
#include "base/bind.h"
#include "base/fs.h"
#include "she/display.h"
#include "she/system.h"
#include "ui/message.h"
#include "ui/splitter.h"
#include "ui/system.h"
#include "ui/view.h"

#ifdef ENABLE_SCRIPTING
  #include "app/ui/devconsole_view.h"
#endif

namespace app {

using namespace ui;

class ScreenScalePanic : public INotificationDelegate {
public:
  std::string notificationText() override {
    return "Reset Scale!";
  }

  void notificationClick() override {
    auto& pref = Preferences::instance();

    const int newScreenScale = 2;
    const int newUIScale = 1;

    if (pref.general.screenScale() != newScreenScale)
      pref.general.screenScale(newScreenScale);

    if (pref.general.uiScale() != newUIScale)
      pref.general.uiScale(newUIScale);

    pref.save();

    ui::set_theme(ui::get_theme(), newUIScale);

    Manager* manager = Manager::getDefault();
    she::Display* display = manager->getDisplay();
    display->setScale(newScreenScale);
    manager->setDisplay(display);
  }
};

MainWindow::MainWindow()
  : m_mode(NormalMode)
  , m_homeView(nullptr)
  , m_scalePanic(nullptr)
  , m_browserView(nullptr)
#ifdef ENABLE_SCRIPTING
  , m_devConsoleView(nullptr)
#endif
{
  m_menuBar = new MainMenuBar();
  m_notifications = new Notifications();
  m_contextBar = new ContextBar();
  m_statusBar = new StatusBar();
  m_colorBar = new ColorBar(colorBarPlaceholder()->align());
  m_toolBar = new ToolBar();
  m_tabsBar = new WorkspaceTabs(this);
  m_workspace = new Workspace();
  m_previewEditor = new PreviewEditorWindow();
  m_stageView = new StageView();
  m_timeline = new Timeline();

  Editor::registerCommands();

  m_workspace->setTabsBar(m_tabsBar);
  m_workspace->ActiveViewChanged.connect(&MainWindow::onActiveViewChange, this);

  // configure all widgets to expansives
  m_menuBar->setExpansive(true);
  m_contextBar->setExpansive(true);
  m_contextBar->setVisible(false);
  m_statusBar->setExpansive(true);
  m_colorBar->setExpansive(true);
  m_toolBar->setExpansive(true);
  m_tabsBar->setExpansive(true);
  m_timeline->setExpansive(true);
  m_workspace->setExpansive(true);
  m_notifications->setVisible(false);

  // Load all menus by first time.
  AppMenus::instance()->reload();

  // Setup the menus
  m_menuBar->setMenu(AppMenus::instance()->getRootMenu());

  // Add the widgets in the boxes
  menuBarPlaceholder()->addChild(m_menuBar);
  menuBarPlaceholder()->addChild(m_notifications);
  contextBarPlaceholder()->addChild(m_contextBar);
  colorBarPlaceholder()->addChild(m_colorBar);
  toolBarPlaceholder()->addChild(m_toolBar);
  statusBarPlaceholder()->addChild(m_statusBar);
  tabsPlaceholder()->addChild(m_tabsBar);
  workspacePlaceholder()->addChild(m_workspace);
  timelinePlaceholder()->addChild(m_timeline);

  // Default splitter positions
  colorBarSplitter()->setPosition(m_colorBar->sizeHint().w);
  timelineSplitter()->setPosition(75);

  // Reconfigure workspace when the timeline position is changed.
  auto& pref = Preferences::instance();
  pref.general.timelinePosition
    .AfterChange.connect(base::Bind<void>(&MainWindow::configureWorkspaceLayout, this));
  pref.general.showMenuBar
    .AfterChange.connect(base::Bind<void>(&MainWindow::configureWorkspaceLayout, this));

  // Prepare the window
  remapWindow();

  AppMenus::instance()->rebuildRecentList();

  // When the language is change, we reload the menu bar strings and
  // relayout the whole main window.
  Strings::instance()->LanguageChange.connect(
    [this]{
      m_menuBar->reload();
      layout();
      invalidate();
    });
}

MainWindow::~MainWindow()
{
  delete m_scalePanic;

#ifdef ENABLE_SCRIPTING
  if (m_devConsoleView) {
    if (m_devConsoleView->parent())
      m_workspace->removeView(m_devConsoleView);
    delete m_devConsoleView;
  }
#endif

  if (m_browserView) {
    if (m_browserView->parent())
      m_workspace->removeView(m_browserView);
    delete m_browserView;
  }

  if (m_homeView) {
    if (m_homeView->parent())
      m_workspace->removeView(m_homeView);
    delete m_homeView;
  }
  delete m_contextBar;
  if (m_stageView) {
    if (m_stageView->parent())
      m_workspace->removeView(m_stageView);
    delete m_stageView;
  }
  delete m_previewEditor;

  // Destroy the workspace first so ~Editor can dettach slots from
  // ColorBar. TODO this is a terrible hack for slot/signal stuff,
  // connections should be handle in a better/safer way.
  delete m_workspace;

  // Remove the root-menu from the menu-bar (because the rootmenu
  // module should destroy it).
  m_menuBar->setMenu(NULL);
}

DocumentView* MainWindow::getDocView()
{
  return dynamic_cast<DocumentView*>(m_workspace->activeView());
}

HomeView* MainWindow::getHomeView()
{
  if (!m_homeView)
    m_homeView = new HomeView;
  return m_homeView;
}

#ifdef ENABLE_UPDATER
CheckUpdateDelegate* MainWindow::getCheckUpdateDelegate()
{
  return getHomeView();
}
#endif

void MainWindow::showNotification(INotificationDelegate* del)
{
  m_notifications->addLink(del);
  m_notifications->setVisible(true);
  m_notifications->parent()->layout();
}

void MainWindow::showHomeOnOpen()
{
  // Don't open Home tab
  if (!Preferences::instance().general.showHome()) {
    configureWorkspaceLayout();
    return;
  }

  if (!getHomeView()->parent()) {
    TabView* selectedTab = m_tabsBar->getSelectedTab();

    // Show "Home" tab in the first position, and select it only if
    // there is no other view selected.
    m_workspace->addView(m_homeView, 0);
    if (selectedTab)
      m_tabsBar->selectTab(selectedTab);
    else
      m_tabsBar->selectTab(m_homeView);
  }
}

void MainWindow::showHome()
{
  if (!getHomeView()->parent()) {
    m_workspace->addView(m_homeView, 0);
  }
  m_tabsBar->selectTab(m_homeView);
}

bool MainWindow::isHomeSelected()
{
  return (m_tabsBar->getSelectedTab() == m_homeView && m_homeView);
}

void MainWindow::showStage()
{
  if (!getStageView()->parent()) {
    m_workspace->addView(m_stageView, 0);
  }
  m_tabsBar->selectTab(m_stageView);
}

bool MainWindow::isStageSelected()
{
  return (m_tabsBar->getSelectedTab() == m_stageView && m_stageView);
}

void MainWindow::showBrowser(const std::string& filename)
{
  if (!m_browserView)
    m_browserView = new BrowserView;

  m_browserView->loadFile(filename);

  if (!m_browserView->parent()) {
    m_workspace->addView(m_browserView);
    m_tabsBar->selectTab(m_browserView);
  }
}

void MainWindow::showDevConsole()
{
#ifdef ENABLE_SCRIPTING
  if (!m_devConsoleView)
    m_devConsoleView = new DevConsoleView;

  if (!m_devConsoleView->parent()) {
    m_workspace->addView(m_devConsoleView);
    m_tabsBar->selectTab(m_devConsoleView);
  }
#endif
}

void MainWindow::setMode(Mode mode)
{
  // Check if we already are in the given mode.
  if (m_mode == mode)
    return;

  m_mode = mode;
  configureWorkspaceLayout();
}

bool MainWindow::getTimelineVisibility() const
{
  return Preferences::instance().general.visibleTimeline();
}

void MainWindow::setTimelineVisibility(bool visible)
{
  Preferences::instance().general.visibleTimeline(visible);

  configureWorkspaceLayout();
}

void MainWindow::popTimeline()
{
  Preferences& preferences = Preferences::instance();

  if (!preferences.general.autoshowTimeline())
    return;

  if (!getTimelineVisibility())
    setTimelineVisibility(true);
}

void MainWindow::showDataRecovery(crash::DataRecovery* dataRecovery)
{
  getHomeView()->showDataRecovery(dataRecovery);
}

bool MainWindow::onProcessMessage(ui::Message* msg)
{
  if (msg->type() == kOpenMessage)
    showHomeOnOpen();

  return Window::onProcessMessage(msg);
}

void MainWindow::onInitTheme(ui::InitThemeEvent& ev)
{
  app::gen::MainWindow::onInitTheme(ev);
  if (m_previewEditor)
    m_previewEditor->initTheme();
}

void MainWindow::onSaveLayout(SaveLayoutEvent& ev)
{
  // Invert the timeline splitter position before we save the setting.
  if (Preferences::instance().general.timelinePosition() ==
      gen::TimelinePosition::LEFT) {
    timelineSplitter()->setPosition(100 - timelineSplitter()->getPosition());
  }

  Window::onSaveLayout(ev);
}

void MainWindow::onResize(ui::ResizeEvent& ev)
{
  app::gen::MainWindow::onResize(ev);

  she::Display* display = manager()->getDisplay();
  if ((display) &&
      (display->scale()*ui::guiscale() > 2) &&
      (!m_scalePanic) &&
      (ui::display_w()/ui::guiscale() < 320 ||
       ui::display_h()/ui::guiscale() < 260)) {
    showNotification(m_scalePanic = new ScreenScalePanic);
  }
}

// When the active view is changed from methods like
// Workspace::splitView(), this function is called, and we have to
// inform to the UIContext that the current view has changed.
void MainWindow::onActiveViewChange()
{
  if (DocumentView* docView = getDocView())
    UIContext::instance()->setActiveView(docView);
  else
    UIContext::instance()->setActiveView(nullptr);

  configureWorkspaceLayout();
}

bool MainWindow::isTabModified(Tabs* tabs, TabView* tabView)
{
  if (DocumentView* docView = dynamic_cast<DocumentView*>(tabView)) {
    Document* document = docView->document();
    return document->isModified();
  }
  else {
    return false;
  }
}

bool MainWindow::canCloneTab(Tabs* tabs, TabView* tabView)
{
  ASSERT(tabView)

  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  return view->canCloneWorkspaceView();
}

void MainWindow::onSelectTab(Tabs* tabs, TabView* tabView)
{
  if (!tabView)
    return;

  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  if (m_workspace->activeView() != view)
    m_workspace->setActiveView(view);
}

void MainWindow::onCloseTab(Tabs* tabs, TabView* tabView)
{
  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  ASSERT(view);
  if (view)
    m_workspace->closeView(view, false);
}

void MainWindow::onCloneTab(Tabs* tabs, TabView* tabView, int pos)
{
  EditorView::SetScrollUpdateMethod(EditorView::KeepOrigin);

  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  WorkspaceView* clone = view->cloneWorkspaceView();
  ASSERT(clone);

  m_workspace->addViewToPanel(
    static_cast<WorkspaceTabs*>(tabs)->panel(), clone, true, pos);

  clone->onClonedFrom(view);
}

void MainWindow::onContextMenuTab(Tabs* tabs, TabView* tabView)
{
  WorkspaceView* view = dynamic_cast<WorkspaceView*>(tabView);
  ASSERT(view);
  if (view)
    view->onTabPopup(m_workspace);
}

void MainWindow::onTabsContainerDoubleClicked(Tabs* tabs)
{
  WorkspacePanel* mainPanel = m_workspace->mainPanel();
  WorkspaceView* oldActiveView = mainPanel->activeView();
  app::Document* oldDoc = static_cast<app::Document*>(UIContext::instance()->activeDocument());

  Command* command = Commands::instance()->byId(CommandId::NewFile());
  UIContext::instance()->executeCommand(command);

  app::Document* newDoc = static_cast<app::Document*>(UIContext::instance()->activeDocument());
  if (newDoc != oldDoc) {
    WorkspacePanel* doubleClickedPanel =
      static_cast<WorkspaceTabs*>(tabs)->panel();

    // TODO move this code to workspace?
    // Put the new sprite in the double-clicked tabs control
    if (doubleClickedPanel != mainPanel) {
      WorkspaceView* newView = m_workspace->activeView();
      m_workspace->removeView(newView);
      m_workspace->addViewToPanel(doubleClickedPanel, newView, false, -1);

      // Re-activate the old view in the main panel
      mainPanel->setActiveView(oldActiveView);
      doubleClickedPanel->setActiveView(newView);
    }
  }
}

void MainWindow::onMouseOverTab(Tabs* tabs, TabView* tabView)
{
  // Note: tabView can be NULL
  if (DocumentView* docView = dynamic_cast<DocumentView*>(tabView)) {
    Document* document = docView->document();

    std::string name;
    if (Preferences::instance().general.showFullPath())
      name = document->filename();
    else
      name = base::get_file_name(document->filename());

    m_statusBar->setStatusText(250, "%s", name.c_str());
  }
  else {
    m_statusBar->clearText();
  }
}

DropViewPreviewResult MainWindow::onFloatingTab(Tabs* tabs, TabView* tabView, const gfx::Point& pos)
{
  return m_workspace->setDropViewPreview(pos,
    dynamic_cast<WorkspaceView*>(tabView),
    static_cast<WorkspaceTabs*>(tabs));
}

void MainWindow::onDockingTab(Tabs* tabs, TabView* tabView)
{
  m_workspace->removeDropViewPreview();
}

DropTabResult MainWindow::onDropTab(Tabs* tabs, TabView* tabView, const gfx::Point& pos, bool clone)
{
  m_workspace->removeDropViewPreview();

  DropViewAtResult result =
    m_workspace->dropViewAt(pos, dynamic_cast<WorkspaceView*>(tabView), clone);

  if (result == DropViewAtResult::MOVED_TO_OTHER_PANEL)
    return DropTabResult::REMOVE;
  else if (result == DropViewAtResult::CLONED_VIEW)
    return DropTabResult::DONT_REMOVE;
  else
    return DropTabResult::NOT_HANDLED;
}

void MainWindow::configureWorkspaceLayout()
{
  const auto& pref = Preferences::instance();
  bool normal = (m_mode == NormalMode);
  bool isDoc = (getDocView() != nullptr);
  bool isStageViewShown = (getStageView() != nullptr && getStageView()->isVisible());

  if (she::instance()->menus() == nullptr ||
      pref.general.showMenuBar()) {
    if (!m_menuBar->parent())
      menuBarPlaceholder()->insertChild(0, m_menuBar);
  }
  else {
    if (m_menuBar->parent())
      menuBarPlaceholder()->removeChild(m_menuBar);
  }

  m_menuBar->setVisible(normal);
  m_tabsBar->setVisible(normal);
  colorBarPlaceholder()->setVisible(normal && isDoc);
  m_toolBar->setVisible(normal && isDoc);
  m_statusBar->setVisible(normal);
  m_contextBar->setVisible(
    isDoc &&
    (m_mode == NormalMode ||
     m_mode == ContextBarAndTimelineMode));

  // Configure timeline
  {
    auto timelinePosition = pref.general.timelinePosition();
    bool invertWidgets = false;
    int align = VERTICAL;
    switch (timelinePosition) {
      case gen::TimelinePosition::LEFT:
        align = HORIZONTAL;
        invertWidgets = true;
        break;
      case gen::TimelinePosition::RIGHT:
        align = HORIZONTAL;
        break;
      case gen::TimelinePosition::BOTTOM:
        break;
    }

    timelineSplitter()->setAlign(align);
    timelinePlaceholder()->setVisible(
      (
        isDoc &&
          (m_mode == NormalMode ||
           m_mode == ContextBarAndTimelineMode)
        || isStageViewShown
      ) &&
      pref.general.visibleTimeline());

    bool invertSplitterPos = false;
    if (invertWidgets) {
      if (timelineSplitter()->firstChild() == workspacePlaceholder() &&
          timelineSplitter()->lastChild() == timelinePlaceholder()) {
        timelineSplitter()->removeChild(workspacePlaceholder());
        timelineSplitter()->addChild(workspacePlaceholder());
        invertSplitterPos = true;
      }
    }
    else {
      if (timelineSplitter()->firstChild() == timelinePlaceholder() &&
          timelineSplitter()->lastChild() == workspacePlaceholder()) {
        timelineSplitter()->removeChild(timelinePlaceholder());
        timelineSplitter()->addChild(timelinePlaceholder());
        invertSplitterPos = true;
      }
    }
    if (invertSplitterPos)
      timelineSplitter()->setPosition(100 - timelineSplitter()->getPosition());
  }

  if (m_contextBar->isVisible()) {
    m_contextBar->updateForActiveTool();
  }

  layout();
  invalidate();
}

} // namespace app
