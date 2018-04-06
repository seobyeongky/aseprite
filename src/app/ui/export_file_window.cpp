// Aseprite
// Copyright (C) 2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/export_file_window.h"

#include "app/document.h"
#include "app/i18n/strings.h"
#include "app/ui/layer_frame_comboboxes.h"
#include "app/ui_context.h"
#include "base/bind.h"
#include "base/convert_to.h"
#include "base/fs.h"
#include "doc/frame_tag.h"
#include "doc/selected_frames.h"
#include "doc/site.h"
#include "fmt/format.h"

namespace app {

ExportFileWindow::ExportFileWindow(const Document* doc)
  : m_doc(doc)
  , m_docPref(Preferences::instance().document(doc))
  , m_preferredResize(1)
{
  auto& pref = Preferences::instance();

  // Is a default output filename in the preferences?
  if (!m_docPref.saveCopy.filename().empty()) {
    setOutputFilename(m_docPref.saveCopy.filename());
  }
  else {
    std::string newFn = base::replace_extension(
      doc->filename(),
      (doc->sprite()->totalFrames() > 1 ? pref.exportFile.animationDefaultExtension():
                                          pref.exportFile.imageDefaultExtension()));
    if (newFn == doc->filename()) {
      newFn = base::join_path(
        base::get_file_path(newFn),
        base::get_file_title(newFn) + "-export." + base::get_file_extension(newFn));
    }
    setOutputFilename(newFn);
  }

  // Default export configuration
  resize()->setValue(
    base::convert_to<std::string>(m_docPref.saveCopy.resizeScale()));
  fill_layers_combobox(m_doc->sprite(), layers(), m_docPref.saveCopy.layer());
  fill_frames_combobox(m_doc->sprite(), frames(), m_docPref.saveCopy.frameTag());
  fill_anidir_combobox(anidir(), m_docPref.saveCopy.aniDir());
  pixelRatio()->setSelected(m_docPref.saveCopy.applyPixelRatio());
  forTwitter()->setSelected(m_docPref.saveCopy.forTwitter());
  adjustResize()->setVisible(false);

  updateAniDir();
  updateAdjustResizeButton();

  outputFilename()->Change.connect(
    base::Bind<void>(
      [this]{
        m_outputFilename = outputFilename()->text();
        onOutputFilenameEntryChange();
      }));
  outputFilenameBrowse()->Click.connect(
    base::Bind<void>(
      [this]{
        std::string fn = SelectOutputFile();
        if (!fn.empty()) {
          setOutputFilename(fn);
        }
      }));

  resize()->Change.connect(base::Bind<void>(&ExportFileWindow::updateAdjustResizeButton, this));
  frames()->Change.connect(base::Bind<void>(&ExportFileWindow::updateAniDir, this));
  forTwitter()->Click.connect(base::Bind<void>(&ExportFileWindow::updateAdjustResizeButton, this));
  adjustResize()->Click.connect(base::Bind<void>(&ExportFileWindow::onAdjustResize, this));
}

bool ExportFileWindow::show()
{
  openWindowInForeground();
  return (closer() == ok());
}

void ExportFileWindow::savePref()
{
  m_docPref.saveCopy.filename(outputFilenameValue());
  m_docPref.saveCopy.resizeScale(resizeValue());
  m_docPref.saveCopy.layer(layersValue());
  m_docPref.saveCopy.frameTag(framesValue());
  m_docPref.saveCopy.applyPixelRatio(applyPixelRatio());
  m_docPref.saveCopy.forTwitter(isForTwitter());
}

std::string ExportFileWindow::outputFilenameValue() const
{
  return base::join_path(m_outputPath,
                         m_outputFilename);
}

double ExportFileWindow::resizeValue() const
{
  return base::convert_to<double>(resize()->getValue());
}

std::string ExportFileWindow::layersValue() const
{
  return layers()->getValue();
}

std::string ExportFileWindow::framesValue() const
{
  return frames()->getValue();
}

doc::AniDir ExportFileWindow::aniDirValue() const
{
  return (doc::AniDir)anidir()->getSelectedItemIndex();
}

bool ExportFileWindow::applyPixelRatio() const
{
  return pixelRatio()->isSelected();
}

bool ExportFileWindow::isForTwitter() const
{
  return forTwitter()->isSelected();
}

void ExportFileWindow::setOutputFilename(const std::string& pathAndFilename)
{
  m_outputPath = base::get_file_path(pathAndFilename);
  m_outputFilename = base::get_file_name(pathAndFilename);

  updateOutputFilenameEntry();
}

void ExportFileWindow::updateOutputFilenameEntry()
{
  outputFilename()->setText(m_outputFilename);
  onOutputFilenameEntryChange();
}

void ExportFileWindow::onOutputFilenameEntryChange()
{
  ok()->setEnabled(!m_outputFilename.empty());
}

void ExportFileWindow::updateAniDir()
{
  std::string framesValue = this->framesValue();
  if (!framesValue.empty() &&
      framesValue != kAllFrames &&
      framesValue != kSelectedFrames) {
    SelectedFrames selFrames;
    FrameTag* frameTag = calculate_selected_frames(
      UIContext::instance()->activeSite(), framesValue, selFrames);
    if (frameTag)
      anidir()->setSelectedItemIndex(int(frameTag->aniDir()));
  }
  else
    anidir()->setSelectedItemIndex(int(doc::AniDir::FORWARD));
}

void ExportFileWindow::updateAdjustResizeButton()
{
  // Calculate a better size for Twitter
  m_preferredResize = 1;
  while (m_preferredResize < 10 &&
         (m_doc->width()*m_preferredResize < 240 ||
          m_doc->height()*m_preferredResize < 240)) {
    ++m_preferredResize;
  }

  const bool newState =
    forTwitter()->isSelected() &&
    ((int)resizeValue() < m_preferredResize);

  if (adjustResize()->isVisible() != newState) {
    adjustResize()->setVisible(newState);
    if (newState)
      adjustResize()->setText(fmt::format(Strings::export_file_adjust_resize(),
                                          100 * m_preferredResize));
    adjustResize()->parent()->layout();
  }
}

void ExportFileWindow::onAdjustResize()
{
  resize()->setValue(base::convert_to<std::string>(m_preferredResize));

  adjustResize()->setVisible(false);
  adjustResize()->parent()->layout();
}

} // namespace app
