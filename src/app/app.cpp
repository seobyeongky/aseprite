// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"

#include "app/check_update.h"
#include "app/cli/app_options.h"
#include "app/cli/cli_processor.h"
#include "app/cli/default_cli_delegate.h"
#include "app/cli/preview_cli_delegate.h"
#include "app/color_utils.h"
#include "app/commands/commands.h"
#include "app/console.h"
#include "app/crash/data_recovery.h"
#include "app/extensions.h"
#include "app/file/file.h"
#include "app/file/file_formats_manager.h"
#include "app/file_system.h"
#include "app/gui_xml.h"
#include "app/i18n/strings.h"
#include "app/ini_file.h"
#include "app/log.h"
#include "app/modules.h"
#include "app/modules/gfx.h"
#include "app/modules/gui.h"
#include "app/modules/palettes.h"
#include "app/pref/preferences.h"
#include "app/recent_files.h"
#include "app/resource_finder.h"
#include "app/send_crash.h"
#include "app/tools/active_tool.h"
#include "app/tools/tool_box.h"
#include "app/ui/backup_indicator.h"
#include "app/ui/color_bar.h"
#include "app/ui/document_view.h"
#include "app/ui/editor/editor.h"
#include "app/ui/editor/editor_view.h"
#include "app/ui/input_chain.h"
#include "app/ui/keyboard_shortcuts.h"
#include "app/ui/main_window.h"
#include "app/ui/status_bar.h"
#include "app/ui/toolbar.h"
#include "app/ui/workspace.h"
#include "app/ui_context.h"
#include "app/util/clipboard.h"
#include "app/webserver.h"
#include "base/exception.h"
#include "base/fs.h"
#include "base/scoped_lock.h"
#include "base/split_string.h"
#include "base/unique_ptr.h"
#include "doc/site.h"
#include "doc/sprite.h"
#include "fmt/format.h"
#include "render/render.h"
#include "she/display.h"
#include "she/error.h"
#include "she/surface.h"
#include "she/system.h"
#include "ui/intern.h"
#include "ui/ui.h"

#include <iostream>

#ifdef ENABLE_SCRIPTING
  #include "app/script/app_scripting.h"
  #include "app/shell.h"
  #include "script/engine_delegate.h"
#endif

#ifdef ENABLE_STEAM
  #include "steam/steam.h"
#endif

namespace app {

using namespace ui;

class App::CoreModules {
public:
  ConfigModule m_configModule;
  Preferences m_preferences;
};

class App::LoadLanguage {
public:
  LoadLanguage(Preferences& pref,
               Extensions& exts) {
    Strings::createInstance(pref, exts);
  }
};

class App::Modules {
public:
  LoggerModule m_loggerModule;
  FileSystemModule m_file_system_module;
  Extensions m_extensions;
  // Load main language (after loading the extensions)
  LoadLanguage m_loadLanguage;
  tools::ToolBox m_toolbox;
  tools::ActiveToolManager m_activeToolManager;
  Commands m_commands;
  UIContext m_ui_context;
  RecentFiles m_recent_files;
  InputChain m_inputChain;
  clipboard::ClipboardManager m_clipboardManager;
  // This is a raw pointer because we want to delete this explicitly.
  app::crash::DataRecovery* m_recovery;

  Modules(const bool createLogInDesktop,
          Preferences& pref)
    : m_loggerModule(createLogInDesktop)
    , m_loadLanguage(pref, m_extensions)
    , m_activeToolManager(&m_toolbox)
    , m_recent_files(pref.general.recentItems())
    , m_recovery(nullptr) {
  }

  app::crash::DataRecovery* recovery() {
    return m_recovery;
  }

  bool hasRecoverySessions() const {
    return m_recovery && !m_recovery->sessions().empty();
  }

  void createDataRecovery() {
#ifdef ENABLE_DATA_RECOVERY
    m_recovery = new app::crash::DataRecovery(&m_ui_context);
#endif
  }

  void deleteDataRecovery() {
#ifdef ENABLE_DATA_RECOVERY
    delete m_recovery;
    m_recovery = nullptr;
#endif
  }

};

App* App::m_instance = NULL;

App::App()
  : m_coreModules(NULL)
  , m_modules(NULL)
  , m_legacy(NULL)
  , m_isGui(false)
  , m_isShell(false)
  , m_backupIndicator(nullptr)
{
  ASSERT(m_instance == NULL);
  m_instance = this;
}

void App::initialize(const AppOptions& options)
{
#ifdef _WIN32
  if (options.disableWintab())
    she::instance()->useWintabAPI(false);
#endif

  m_isGui = options.startUI() && !options.previewCLI();
  m_isShell = options.startShell();
  m_coreModules = new CoreModules;
  if (m_isGui)
    m_uiSystem.reset(new ui::UISystem);

  bool createLogInDesktop = false;
  switch (options.verboseLevel()) {
    case AppOptions::kNoVerbose:
      base::set_log_level(ERROR);
      break;
    case AppOptions::kVerbose:
      base::set_log_level(INFO);
      break;
    case AppOptions::kHighlyVerbose:
      base::set_log_level(VERBOSE);
      createLogInDesktop = true;
      break;
  }

  // Load modules
  m_modules = new Modules(createLogInDesktop, preferences());
  m_legacy = new LegacyModules(isGui() ? REQUIRE_INTERFACE: 0);
  m_brushes.reset(new AppBrushes);

  // Data recovery is enabled only in GUI mode
  if (isGui() && preferences().general.dataRecovery())
    m_modules->createDataRecovery();

  if (isPortable())
    LOG("APP: Running in portable mode\n");

  // Load or create the default palette, or migrate the default
  // palette from an old format palette to the new one, etc.
  load_default_palette();

  // Initialize GUI interface
  if (isGui()) {
    LOG("APP: GUI mode\n");

    // Setup the GUI cursor and redraw screen
    ui::set_use_native_cursors(preferences().cursor.useNativeCursor());
    ui::set_mouse_cursor_scale(preferences().cursor.cursorScale());
    ui::set_mouse_cursor(kArrowCursor);

    ui::Manager::getDefault()->invalidate();

    // Create the main window and show it.
    m_mainWindow.reset(new MainWindow);

    // Default status of the main window.
    app_rebuild_documents_tabs();
    app_default_statusbar_message();

    // Recover data
    if (m_modules->hasRecoverySessions())
      m_mainWindow->showDataRecovery(m_modules->recovery());

    m_mainWindow->openWindow();

    // Redraw the whole screen.
    ui::Manager::getDefault()->invalidate();
  }

  // Process options
  LOG("APP: Processing options...\n");
  {
    base::UniquePtr<CliDelegate> delegate;
    if (options.previewCLI())
      delegate.reset(new PreviewCliDelegate);
    else
      delegate.reset(new DefaultCliDelegate);

    CliProcessor cli(delegate.get(), options);
    cli.process();
  }

  she::instance()->finishLaunching();
}

void App::run()
{
  // Run the GUI
  if (isGui()) {
#if !defined(_WIN32) && !defined(__APPLE__)
    // Setup app icon for Linux window managers
    try {
      she::Display* display = she::instance()->defaultDisplay();
      she::SurfaceList icons;

      for (const int size : { 32, 64, 128 }) {
        ResourceFinder rf;
        rf.includeDataDir(fmt::format("icons/ase{0}.png", size).c_str());
        if (rf.findFirst()) {
          she::Surface* surf = she::instance()->loadRgbaSurface(rf.filename().c_str());
          if (surf)
            icons.push_back(surf);
        }
      }

      display->setIcons(icons);

      for (auto surf : icons)
        surf->dispose();
    }
    catch (const std::exception&) {
      // Just ignore the exception, we couldn't change the app icon, no
      // big deal.
    }
#endif

    // Initialize Steam API
#ifdef ENABLE_STEAM
    steam::SteamAPI steam;
    if (steam.initialized())
      she::instance()->activateApp();
#endif

#if ENABLE_DEVMODE
    // On OS X, when we compile Aseprite on devmode, we're using it
    // outside an app bundle, so we must active the app explicitly.
    she::instance()->activateApp();
#endif

#ifdef ENABLE_UPDATER
    // Launch the thread to check for updates.
    app::CheckUpdateThreadLauncher checkUpdate(
      m_mainWindow->getCheckUpdateDelegate());
    checkUpdate.launch();
#endif

#ifdef ENABLE_WEBSERVER
    // Launch the webserver.
    app::WebServer webServer;
    webServer.start();
#endif

    app::SendCrash sendCrash;
    sendCrash.search();

    // Run the GUI main message loop
    ui::Manager::getDefault()->run();
  }

#ifdef ENABLE_SCRIPTING
  // Start shell to execute scripts.
  if (m_isShell) {
    script::StdoutEngineDelegate delegate;
    AppScripting engine(&delegate);
    engine.printLastResult();
    Shell shell;
    shell.run(engine);
  }
#endif

  // Destroy all documents in the UIContext.
  const doc::Documents& docs = m_modules->m_ui_context.documents();
  while (!docs.empty()) {
    doc::Document* doc = docs.back();

    // First we close the document. In this way we receive recent
    // notifications related to the document as an app::Document. If
    // we delete the document directly, we destroy the app::Document
    // too early, and then doc::~Document() call
    // DocumentsObserver::onRemoveDocument(). In this way, observers
    // could think that they have a fully created app::Document when
    // in reality it's a doc::Document (in the middle of a
    // destruction process).
    //
    // TODO: This problem is because we're extending doc::Document,
    // in the future, we should remove app::Document.
    doc->close();
    delete doc;
  }

  if (isGui()) {
    // Destroy the window.
    m_mainWindow.reset(NULL);
  }

  // Delete backups (this is a normal shutdown, we are not handling
  // exceptions, and we are not in a destructor).
  m_modules->deleteDataRecovery();
}

// Finishes the Aseprite application.
App::~App()
{
  try {
    LOG("APP: Exit\n");
    ASSERT(m_instance == this);

    // Delete file formats.
    FileFormatsManager::destroyInstance();

    // Fire App Exit signal.
    App::instance()->Exit();

    // Finalize modules, configuration and core.
    Editor::destroyEditorSharedInternals();

    // Save brushes
    m_brushes.reset(nullptr);

    if (m_backupIndicator) {
      delete m_backupIndicator;
      m_backupIndicator = nullptr;
    }

    delete m_legacy;
    delete m_modules;
    delete m_coreModules;

    // Destroy the loaded gui.xml data.
    delete KeyboardShortcuts::instance();
    delete GuiXml::instance();

    m_instance = NULL;
  }
  catch (const std::exception& e) {
    LOG(ERROR) << "APP: Error: " << e.what() << "\n";
    she::error_message(e.what());

    // no re-throw
  }
  catch (...) {
    she::error_message("Error closing " PACKAGE ".\n(uncaught exception)");

    // no re-throw
  }
}

bool App::isPortable()
{
  static bool* is_portable = NULL;
  if (!is_portable) {
    is_portable =
      new bool(
        base::is_file(base::join_path(
            base::get_file_path(base::get_app_path()),
            "aseprite.ini")));
  }
  return *is_portable;
}

tools::ToolBox* App::toolBox() const
{
  ASSERT(m_modules != NULL);
  return &m_modules->m_toolbox;
}

tools::Tool* App::activeTool() const
{
  return m_modules->m_activeToolManager.activeTool();
}

tools::ActiveToolManager* App::activeToolManager() const
{
  return &m_modules->m_activeToolManager;
}

RecentFiles* App::recentFiles() const
{
  ASSERT(m_modules != NULL);
  return &m_modules->m_recent_files;
}

Workspace* App::workspace() const
{
  if (m_mainWindow)
    return m_mainWindow->getWorkspace();
  else
    return nullptr;
}

ContextBar* App::contextBar() const
{
  if (m_mainWindow)
    return m_mainWindow->getContextBar();
  else
    return nullptr;
}

Timeline* App::timeline() const
{
  if (m_mainWindow)
    return m_mainWindow->getTimeline();
  else
    return nullptr;
}

Preferences& App::preferences() const
{
  return m_coreModules->m_preferences;
}

Extensions& App::extensions() const
{
  return m_modules->m_extensions;
}

crash::DataRecovery* App::dataRecovery() const
{
  return m_modules->recovery();
}

void App::showNotification(INotificationDelegate* del)
{
  m_mainWindow->showNotification(del);
}

void App::showBackupNotification(bool state)
{
  base::scoped_lock lock(m_backupIndicatorMutex);
  if (state) {
    if (!m_backupIndicator)
      m_backupIndicator = new BackupIndicator;
    m_backupIndicator->start();
  }
  else {
    if (m_backupIndicator)
      m_backupIndicator->stop();
  }
}

void App::updateDisplayTitleBar()
{
  std::string defaultTitle = PACKAGE " v" VERSION;
  std::string title;

  DocumentView* docView = UIContext::instance()->activeView();
  if (docView) {
    // Prepend the document's filename.
    title += docView->document()->name();
    title += " - ";
  }

  title += defaultTitle;
  she::instance()->defaultDisplay()->setTitleBar(title);
}

InputChain& App::inputChain()
{
  return m_modules->m_inputChain;
}

// Updates palette and redraw the screen.
void app_refresh_screen()
{
  Context* context = UIContext::instance();
  ASSERT(context != NULL);

  Site site = context->activeSite();

  if (Palette* pal = site.palette())
    set_current_palette(pal, false);
  else
    set_current_palette(NULL, false);

  // Invalidate the whole screen.
  ui::Manager::getDefault()->invalidate();
}

// TODO remove app_rebuild_documents_tabs() and replace it by
// observable events in the document (so a tab can observe if the
// document is modified).
void app_rebuild_documents_tabs()
{
  if (App::instance()->isGui()) {
    App::instance()->workspace()->updateTabs();
    App::instance()->updateDisplayTitleBar();
  }
}

PixelFormat app_get_current_pixel_format()
{
  Context* context = UIContext::instance();
  ASSERT(context != NULL);

  Document* document = context->activeDocument();
  if (document != NULL)
    return document->sprite()->pixelFormat();
  else
    return IMAGE_RGB;
}

void app_default_statusbar_message()
{
  StatusBar::instance()
    ->setStatusText(250, "%s %s | %s", PACKAGE, VERSION, COPYRIGHT);
}

int app_get_color_to_clear_layer(Layer* layer)
{
  ASSERT(layer != NULL);

  app::Color color;

  // The `Background' is erased with the `Background Color'
  if (layer->isBackground()) {
    if (ColorBar::instance())
      color = ColorBar::instance()->getBgColor();
    else
      color = app::Color::fromRgb(0, 0, 0); // TODO get background color color from doc::Settings
  }
  else // All transparent layers are cleared with the mask color
    color = app::Color::fromMask();

  return color_utils::color_for_layer(color, layer);
}

} // namespace app
