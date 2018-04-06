// Aseprite
// Copyright (C) 2016-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cli/preview_cli_delegate.h"

#include "app/cli/cli_open_file.h"
#include "app/document.h"
#include "app/document_exporter.h"
#include "app/file/file.h"
#include "app/ui_context.h"
#include "base/fs.h"
#include "base/unique_ptr.h"
#include "doc/sprite.h"

#include <iostream>

namespace app {

void PreviewCliDelegate::showHelp(const AppOptions& options)
{
  std::cout << "- Show " PACKAGE " CLI usage\n";
}

void PreviewCliDelegate::showVersion()
{
  std::cout << "- Show " PACKAGE " version\n";
}

void PreviewCliDelegate::uiMode()
{
  std::cout << "- Run UI mode\n";
}

void PreviewCliDelegate::shellMode()
{
  std::cout << "- Run shell mode\n";
}

void PreviewCliDelegate::batchMode()
{
  std::cout << "- Exit\n";
}

void PreviewCliDelegate::beforeOpenFile(const CliOpenFile& cof)
{
  std::cout << "- Open file '" << cof.filename << "'\n";
}

void PreviewCliDelegate::afterOpenFile(const CliOpenFile& cof)
{
  if (!cof.document) {
    std::cout << "  - WARNING: File not found or error loading file\n";
    return;
  }

  if (cof.listLayers)
    std::cout << "  - List layers\n";

  if (cof.listTags)
    std::cout << "  - List tags\n";

  if (cof.listSlices)
    std::cout << "  - List slices\n";

  if (cof.oneFrame)
    std::cout << "  - One frame\n";

  if (cof.allLayers)
    std::cout << "  - Make all layers visible\n";

  showLayersFilter(cof);
}

void PreviewCliDelegate::saveFile(const CliOpenFile& cof)
{
  ASSERT(cof.document);
  ASSERT(cof.document->sprite());

  std::cout << "- Save file '" << cof.filename << "'\n"
            << "  - Sprite: '" << cof.document->filename() << "'\n";

  if (!cof.crop.isEmpty()) {
    std::cout << "  - Crop: "
              << cof.crop.x << ","
              << cof.crop.y << " "
              << cof.crop.w << "x"
              << cof.crop.h << "\n";
  }

  if (cof.trim) {
    std::cout << "  - Trim\n";
  }

  std::cout << "  - Size: "
            << cof.document->sprite()->width() << "x"
            << cof.document->sprite()->height() << "\n";

  showLayersFilter(cof);

  if (cof.hasFrameTag()) {
    std::cout << "  - Frame tag: '" << cof.frameTag << "'\n";
  }

  if (cof.hasSlice()) {
    std::cout << "  - Slice: '" << cof.slice << "'\n";
  }

  if (cof.hasFrameRange()) {
    const auto& selFrames = cof.roi().selectedFrames();
    if (!selFrames.empty()) {
      if (selFrames.ranges() == 1)
        std::cout << "  - Frame range from "
                  << selFrames.firstFrame() << " to "
                  << selFrames.lastFrame() << "\n";
      else {
        std::cout << "  - Specific frames:";
        for (auto frame : selFrames)
          std::cout << ' ' << frame;
        std::cout << "\n";
      }
    }
  }

  if (!cof.filenameFormat.empty())
    std::cout << "  - Filename format: '" << cof.filenameFormat << "'\n";

  base::UniquePtr<FileOp> fop(
    FileOp::createSaveDocumentOperation(
      UIContext::instance(),
      cof.roi(),
      cof.filename,
      cof.filenameFormat));

  if (fop) {
    base::paths files;
    fop->getFilenameList(files);
    for (const auto& file : files) {
      if (base::is_file(file))
        std::cout << "  - Overwrite file: '" << file << "'\n";
      else
        std::cout << "  - Output file: '" << file << "'\n";
    }
  }
  else
    std::cout << "  - No output\n";
}

void PreviewCliDelegate::loadPalette(const CliOpenFile& cof,
                                     const std::string& filename)
{
  ASSERT(cof.document);
  ASSERT(cof.document->sprite());

  std::cout << "- Load palette:\n"
            << "  - Sprite: '" << cof.filename << "'\n"
            << "  - Palette: '" << filename << "'\n";
}

void PreviewCliDelegate::exportFiles(DocumentExporter& exporter)
{
  std::string type = "None";
  switch (exporter.spriteSheetType()) {
    case SpriteSheetType::Horizontal: type = "Horizontal"; break;
    case SpriteSheetType::Vertical:   type = "Vertical";   break;
    case SpriteSheetType::Rows:       type = "Rows";       break;
    case SpriteSheetType::Columns:    type = "Columns";    break;
    case SpriteSheetType::Packed:     type = "Packed";     break;
  }

  gfx::Size size = exporter.calculateSheetSize();
  std::cout << "- Export sprite sheet:\n"
            << "  - Type: " << type << "\n"
            << "  - Size: " << size.w << "x" << size.h << "\n";

  if (!exporter.textureFilename().empty()) {
    std::cout << "  - Save texture file: '"
              << exporter.textureFilename() << "'\n";
  }

  if (!exporter.dataFilename().empty()) {
    std::string format = "Unknown";
    switch (exporter.dataFormat()) {
      case DocumentExporter::JsonHashDataFormat: format = "JSON Hash"; break;
      case DocumentExporter::JsonArrayDataFormat: format = "JSON Array"; break;
    }
    std::cout << "  - Save data file: '" << exporter.dataFilename() << "'\n"
              << "  - Data format: " << format << "\n";

    if (!exporter.filenameFormat().empty()) {
      std::cout << "  - Filename format for JSON items: '"
                << exporter.filenameFormat() << "'\n";
    }
  }
}

void PreviewCliDelegate::execScript(const std::string& filename)
{
  std::cout << "- Run script: '" << filename << "'\n";
}

void PreviewCliDelegate::showLayersFilter(const CliOpenFile& cof)
{
  if (!cof.includeLayers.empty()) {
    std::cout << "  - Include layers:";
    for (const auto& filter : cof.includeLayers)
      std::cout << ' ' << filter;
    std::cout << "\n";
  }

  if (!cof.excludeLayers.empty()) {
    std::cout << "  - Exclude layers:";
    for (const auto& filter : cof.excludeLayers)
      std::cout << ' ' << filter;
    std::cout << "\n";
  }
}

} // namespace app
