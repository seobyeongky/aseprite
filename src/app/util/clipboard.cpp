// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/cmd/clear_mask.h"
#include "app/cmd/deselect_mask.h"
#include "app/cmd/trim_cel.h"
#include "app/console.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/document_api.h"
#include "app/document_range.h"
#include "app/document_range_ops.h"
#include "app/modules/editors.h"
#include "app/modules/gfx.h"
#include "app/modules/gui.h"
#include "app/transaction.h"
#include "app/ui/color_bar.h"
#include "app/ui/editor/editor.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/timeline/timeline.h"
#include "app/ui_context.h"
#include "app/util/clipboard.h"
#include "app/util/clipboard_native.h"
#include "app/util/new_image_from_mask.h"
#include "base/shared_ptr.h"
#include "doc/doc.h"
#include "render/ordered_dither.h"
#include "render/quantization.h"

#include <stdexcept>

namespace app {

namespace {

  class ClipboardRange : public doc::DocumentsObserver {
  public:
    ClipboardRange() : m_doc(nullptr) {
    }

    ~ClipboardRange() {
      ASSERT(!m_doc);
    }

    void observeUIContext() {
      UIContext::instance()->documents().add_observer(this);
    }

    void unobserveUIContext() {
      UIContext::instance()->documents().remove_observer(this);
    }

    bool valid() {
      return (m_doc != nullptr);
    }

    void invalidate() {
      m_doc = nullptr;
    }

    void setRange(Document* doc, const DocumentRange& range) {
      m_doc = doc;
      m_range = range;
    }

    Document* document() const { return m_doc; }
    DocumentRange range() const { return m_range; }

    // DocumentsObserver impl
    void onRemoveDocument(doc::Document* doc) override {
      if (doc == m_doc)
        invalidate();
    }

  private:
    Document* m_doc;
    DocumentRange m_range;
  };

}

namespace clipboard {

using namespace doc;

static base::SharedPtr<Palette> clipboard_palette;
static PalettePicks clipboard_picks;
static ImageRef clipboard_image;
static base::SharedPtr<Mask> clipboard_mask;
static ClipboardRange clipboard_range;

static ClipboardManager* g_instance = nullptr;

ClipboardManager::ClipboardManager()
{
  ASSERT(!g_instance);
  g_instance = this;

  register_native_clipboard_formats();

  clipboard_range.observeUIContext();
}

ClipboardManager::~ClipboardManager()
{
  clipboard_range.invalidate();
  clipboard_range.unobserveUIContext();

  // Clean the whole clipboard
  clipboard_palette.reset();
  clipboard_image.reset();
  clipboard_mask.reset();

  ASSERT(g_instance == this);
  g_instance = nullptr;
}

ClipboardManager* ClipboardManager::instance()
{
  return g_instance;
}

static void set_clipboard_image(Image* image,
                                Mask* mask,
                                Palette* palette,
                                bool set_system_clipboard,
                                bool image_source_is_transparent)
{
  clipboard_palette.reset(palette);
  clipboard_picks.clear();
  clipboard_image.reset(image);
  clipboard_mask.reset(mask);

  // Copy image to the native clipboard
  if (set_system_clipboard) {
    color_t oldMask;
    if (image) {
      oldMask = image->maskColor();
      if (!image_source_is_transparent)
        image->setMaskColor(-1);
    }

    set_native_clipboard_bitmap(image, mask, palette);

    if (image && !image_source_is_transparent)
      image->setMaskColor(oldMask);
  }

  clipboard_range.invalidate();
}

static bool copy_from_document(const Site& site, bool merged = false)
{
  const app::Document* document = static_cast<const app::Document*>(site.document());
  ASSERT(document);

  const Mask* mask = document->mask();
  Image* image = new_image_from_mask(site, mask, merged);
  if (!image)
    return false;

  const Palette* pal = document->sprite()->palette(site.frame());
  set_clipboard_image(
    image,
    (mask ? new Mask(*mask): nullptr),
    (pal ? new Palette(*pal): nullptr),
    true,
    site.layer() && !site.layer()->isBackground());

  return true;
}

ClipboardFormat get_current_format()
{
  // Check if the native clipboard has an image
  if (has_native_clipboard_bitmap())
    return ClipboardImage;
  else if (clipboard_image)
    return ClipboardImage;
  else if (clipboard_range.valid())
    return ClipboardDocumentRange;
  else if (clipboard_palette && clipboard_picks.picks())
    return ClipboardPaletteEntries;
  else
    return ClipboardNone;
}

void get_document_range_info(Document** document, DocumentRange* range)
{
  if (clipboard_range.valid()) {
    *document = clipboard_range.document();
    *range = clipboard_range.range();
  }
  else {
    *document = NULL;
  }
}

void clear_content()
{
  set_clipboard_image(nullptr, nullptr, nullptr, true, false);
}

void cut(ContextWriter& writer)
{
  ASSERT(writer.document() != NULL);
  ASSERT(writer.sprite() != NULL);
  ASSERT(writer.layer() != NULL);

  if (!copy_from_document(*writer.site())) {
    Console console;
    console.printf("Can't copying an image portion from the current layer\n");
  }
  else {
    {
      Transaction transaction(writer.context(), "Cut");
      transaction.execute(new cmd::ClearMask(writer.cel()));

      ASSERT(writer.cel());
      if (writer.cel() &&
          writer.cel()->layer()->isTransparent())
        transaction.execute(new cmd::TrimCel(writer.cel()));

      transaction.execute(new cmd::DeselectMask(writer.document()));
      transaction.commit();
    }
    writer.document()->generateMaskBoundaries();
    update_screen_for_document(writer.document());
  }
}

void copy(const ContextReader& reader)
{
  ASSERT(reader.document() != NULL);

  if (!copy_from_document(*reader.site())) {
    Console console;
    console.printf("Can't copying an image portion from the current layer\n");
    return;
  }
}

void copy_merged(const ContextReader& reader)
{
  ASSERT(reader.document() != NULL);

  copy_from_document(*reader.site(), true);
}

void copy_range(const ContextReader& reader, const DocumentRange& range)
{
  ASSERT(reader.document() != NULL);

  ContextWriter writer(reader);

  clear_content();
  clipboard_range.setRange(writer.document(), range);

  // TODO Replace this with a signal, because here the timeline
  // depends on the clipboard and the clipboard on the timeline.
  App::instance()->timeline()->activateClipboardRange();
}

void copy_image(const Image* image, const Mask* mask, const Palette* pal)
{
  set_clipboard_image(
    Image::createCopy(image),
    (mask ? new Mask(*mask): nullptr),
    (pal ? new Palette(*pal): nullptr),
    true, false);
}

void copy_palette(const Palette* palette, const doc::PalettePicks& picks)
{
  if (!picks.picks())
    return;                     // Do nothing case

  set_clipboard_image(nullptr,
                      nullptr,
                      new Palette(*palette),
                      true, false);
  clipboard_picks = picks;
}

void paste()
{
  Editor* editor = current_editor;
  if (editor == NULL)
    return;

  Document* dstDoc = editor->document();
  Sprite* dstSpr = dstDoc->sprite();

  switch (get_current_format()) {

    case clipboard::ClipboardImage: {
      // Get the image from the clipboard.
      {
        Image* native_image = nullptr;
        Mask* native_mask = nullptr;
        Palette* native_palette = nullptr;
        get_native_clipboard_bitmap(&native_image, &native_mask, &native_palette);
        if (native_image)
          set_clipboard_image(native_image, native_mask, native_palette,
                              false, false);
      }

      if (!clipboard_image)
        return;

      Palette* dst_palette = dstSpr->palette(editor->frame());

      // Source image (clipboard or a converted copy to the destination 'imgtype')
      ImageRef src_image;
      if (clipboard_image->pixelFormat() == dstSpr->pixelFormat() &&
        // Indexed images can be copied directly only if both images
        // have the same palette.
        (clipboard_image->pixelFormat() != IMAGE_INDEXED ||
          clipboard_palette->countDiff(dst_palette, NULL, NULL) == 0)) {
        src_image = clipboard_image;
      }
      else {
        RgbMap* dst_rgbmap = dstSpr->rgbMap(editor->frame());

        src_image.reset(
          render::convert_pixel_format(
            clipboard_image.get(), NULL, dstSpr->pixelFormat(),
            render::DitheringAlgorithm::None,
            render::DitheringMatrix(),
            dst_rgbmap, clipboard_palette.get(),
            false,
            0));
      }

      // Change to MovingPixelsState
      editor->pasteImage(src_image.get(),
                         clipboard_mask.get());
      break;
    }

    case clipboard::ClipboardDocumentRange: {
      DocumentRange srcRange = clipboard_range.range();
      Document* srcDoc = clipboard_range.document();
      Sprite* srcSpr = srcDoc->sprite();

      switch (srcRange.type()) {

        case DocumentRange::kCels: {
          Layer* dstLayer = editor->layer();
          frame_t dstFrameFirst = editor->frame();

          DocumentRange dstRange;
          dstRange.startRange(dstLayer, dstFrameFirst, DocumentRange::kCels);
          for (layer_t i=1; i<srcRange.layers(); ++i) {
            dstLayer = dstLayer->getPreviousBrowsable();
            if (dstLayer == nullptr)
              break;
          }
          dstRange.endRange(dstLayer, dstFrameFirst+srcRange.frames()-1);

          // We can use a document range op (copy_range) to copy/paste
          // cels in the same document.
          if (srcDoc == dstDoc) {
            // This is the app::copy_range (not clipboard::copy_range()).
            if (srcRange.layers() == dstRange.layers())
              app::copy_range(srcDoc, srcRange, dstRange, kDocumentRangeBefore);
            editor->invalidate();
            return;
          }

          Transaction transaction(UIContext::instance(), "Paste Cels");
          DocumentApi api = dstDoc->getApi(transaction);

          // Add extra frames if needed
          while (dstFrameFirst+srcRange.frames() > dstSpr->totalFrames())
            api.addFrame(dstSpr, dstSpr->totalFrames());

          auto srcLayers = srcRange.selectedLayers().toLayerList();
          auto dstLayers = dstRange.selectedLayers().toLayerList();

          auto srcIt = srcLayers.begin();
          auto dstIt = dstLayers.begin();
          auto srcEnd = srcLayers.end();
          auto dstEnd = dstLayers.end();

          for (; srcIt != srcEnd && dstIt != dstEnd; ++srcIt, ++dstIt) {
            auto srcLayer = *srcIt;
            auto dstLayer = *dstIt;

            if (!srcLayer->isImage() ||
                !dstLayer->isImage())
              continue;

            // Maps a linked Cel in the original sprite with its
            // corresponding copy in the new sprite. In this way
            // we can.
            std::map<Cel*, Cel*> relatedCels;

            frame_t dstFrame = dstFrameFirst;
            for (frame_t srcFrame : srcRange.selectedFrames()) {
              Cel* srcCel = srcLayer->cel(srcFrame);
              Cel* srcLink = nullptr;

              if (srcCel && srcCel->image()) {
                bool createCopy = true;

                if (dstLayer->isContinuous() &&
                    srcCel->links()) {
                  srcLink = srcCel->link();
                  if (!srcLink)
                    srcLink = srcCel;

                  if (srcLink) {
                    Cel* dstRelated = relatedCels[srcLink];
                    if (dstRelated) {
                      createCopy = false;

                      // Create a link from dstRelated
                      api.copyCel(
                        static_cast<LayerImage*>(dstLayer), dstRelated->frame(),
                        static_cast<LayerImage*>(dstLayer), dstFrame);
                    }
                  }
                }

                if (createCopy) {
                  api.copyCel(
                    static_cast<LayerImage*>(srcLayer), srcFrame,
                    static_cast<LayerImage*>(dstLayer), dstFrame);

                  if (srcLink)
                    relatedCels[srcLink] = dstLayer->cel(dstFrame);
                }
              }
              else {
                Cel* dstCel = dstLayer->cel(dstFrame);
                if (dstCel)
                  api.clearCel(dstCel);
              }

              ++dstFrame;
            }
          }

          transaction.commit();
          editor->invalidate();
          break;
        }

        case DocumentRange::kFrames: {
          frame_t dstFrame = editor->frame();

          // We use a DocumentRange operation to copy frames inside
          // the same sprite.
          if (srcSpr == dstSpr) {
            DocumentRange dstRange;
            dstRange.startRange(nullptr, dstFrame, DocumentRange::kFrames);
            dstRange.endRange(nullptr, dstFrame);
            app::copy_range(srcDoc, srcRange, dstRange, kDocumentRangeBefore);
            break;
          }

          Transaction transaction(UIContext::instance(), "Paste Frames");
          DocumentApi api = dstDoc->getApi(transaction);

          auto srcLayers = srcSpr->allBrowsableLayers();
          auto dstLayers = dstSpr->allBrowsableLayers();

          for (frame_t srcFrame : srcRange.selectedFrames()) {
            api.addEmptyFrame(dstSpr, dstFrame);
            api.setFrameDuration(dstSpr, dstFrame, srcSpr->frameDuration(srcFrame));
            api.setFrameRootPosition(dstSpr, dstFrame, srcSpr->frameRootPosition(srcFrame));

            auto srcIt = srcLayers.begin();
            auto dstIt = dstLayers.begin();
            auto srcEnd = srcLayers.end();
            auto dstEnd = dstLayers.end();

            for (; srcIt != srcEnd && dstIt != dstEnd; ++srcIt, ++dstIt) {
              auto srcLayer = *srcIt;
              auto dstLayer = *dstIt;

              if (!srcLayer->isImage() ||
                  !dstLayer->isImage())
                continue;

              Cel* cel = static_cast<LayerImage*>(srcLayer)->cel(srcFrame);
              if (cel && cel->image()) {
                api.copyCel(
                  static_cast<LayerImage*>(srcLayer), srcFrame,
                  static_cast<LayerImage*>(dstLayer), dstFrame);
              }
            }

            ++dstFrame;
          }

          transaction.commit();
          editor->invalidate();
          break;
        }

        case DocumentRange::kLayers: {
          if (srcDoc->colorMode() != dstDoc->colorMode())
            throw std::runtime_error("You cannot copy layers of document with different color modes");

          Transaction transaction(UIContext::instance(), "Paste Layers");
          DocumentApi api = dstDoc->getApi(transaction);

          // Remove children if their parent is selected so we only
          // copy the parent.
          SelectedLayers srcLayersSet = srcRange.selectedLayers();
          srcLayersSet.removeChildrenIfParentIsSelected();
          LayerList srcLayers = srcLayersSet.toLayerList();

          // Expand frames of dstDoc if it's needed.
          frame_t maxFrame = 0;
          for (Layer* srcLayer : srcLayers) {
            if (!srcLayer->isImage())
              continue;

            Cel* lastCel = static_cast<LayerImage*>(srcLayer)->getLastCel();
            if (lastCel && maxFrame < lastCel->frame())
              maxFrame = lastCel->frame();
          }
          while (dstSpr->totalFrames() < maxFrame+1)
            api.addEmptyFrame(dstSpr, dstSpr->totalFrames());

          for (Layer* srcLayer : srcLayers) {
            Layer* afterThis;
            if (srcLayer->isBackground() && !dstDoc->sprite()->backgroundLayer())
              afterThis = nullptr;
            else
              afterThis = dstSpr->root()->lastLayer();

            Layer* newLayer = nullptr;
            if (srcLayer->isImage())
              newLayer = new LayerImage(dstSpr);
            else if (srcLayer->isGroup())
              newLayer = new LayerGroup(dstSpr);
            else
              continue;

            api.addLayer(dstSpr->root(), newLayer, afterThis);

            srcDoc->copyLayerContent(srcLayer, dstDoc, newLayer);
          }

          transaction.commit();
          editor->invalidate();
          break;
        }
      }
      break;
    }

  }
}

bool get_image_size(gfx::Size& size)
{
#if defined(_WIN32) || defined(__APPLE__)
  if (get_native_clipboard_bitmap_size(&size))
    return true;
#else
  if (clipboard_image) {
    size.w = clipboard_image->width();
    size.h = clipboard_image->height();
    return true;
  }
#endif

  return false;
}

Palette* get_palette()
{
  if (clipboard::get_current_format() == ClipboardPaletteEntries) {
    ASSERT(clipboard_palette);
    return clipboard_palette.get();
  }
  else
    return nullptr;
}

const PalettePicks& get_palette_picks()
{
  return clipboard_picks;
}

}  // namespace clipboard

} // namespace app
