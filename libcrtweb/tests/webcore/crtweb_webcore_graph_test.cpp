// Web Tranche 3B, milestone B3: WebCore and PAL, built for the CRT platform with the bundled Skia,
// linked into one program that starts WTF, builds a Page-less WebCore object graph (a Document with
// a small element tree), paints rectangles and text through WebCore's GraphicsContext onto a Skia
// surface and reads the pixels back. No Page, no frame, no process model: those are B4/B5.
//
// Run by tools/build_webkit_crt_port.py --run-tests. FONTCONFIG_FILE is set by the driver to a
// configuration that scans libcrtgfx/assets/fonts only.

#include "config.h"

#include <WebCore/ColorSpace.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/Element.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FontCascade.h>
#include <WebCore/FontCascadeDescription.h>
#include <WebCore/FontSelector.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/PixelBufferFormat.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/Settings.h>
#include <WebCore/TextRun.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

#include <stdio.h>

// libwpe is a static library: the embedder supplies the loader. The B3 program has no view
// backend or pasteboard of its own, so no object is provided (PlatformCRT's backend does, in B4).
#include <wpe/wpe.h>
extern "C" {
static void* crtLoadObject(const char*) { return nullptr; }
struct wpe_loader_interface _wpe_loader_interface = { crtLoadObject, nullptr, nullptr, nullptr, nullptr };
}

using namespace WebCore;

static int failures;

static void check(bool condition, const char* what)
{
    if (condition)
        return;
    fprintf(stderr, "crtweb_webcore_graph_test: FAILED %s\n", what);
    ++failures;
}

int main()
{
    JSC::initialize();
    WTF::initializeMainThread();
    // What every WebKit process does first: the generated atom strings and name tables.
    ProcessWarming::initializeNames();

    // The object graph, without a Page: a Document and elements created in it. Inserting nodes
    // is not exercised here: WebCore's insertion steps consult the document's window (custom
    // element registry, autofocus), which a Page-less Document does not have; the full pipeline
    // is milestone B5.
    auto settings = Settings::create(nullptr);
    auto document = Document::create(settings.get(), URL { "https://crt.invalid/"_str });
    auto created = document->createElementForBindings("div"_s);
    check(!created.hasException(), "element creation");
    if (!created.hasException()) {
        Ref element = created.releaseReturnValue();
        check(&element->document() == document.ptr(), "ownerDocument");
        printf("crtweb_webcore_graph_test: element %s\n", element->outerHTML().utf8().data());
        // A Document created without a frame is not an HTMLDocument, so it serializes as XML.
        check(element->outerHTML() == "<div/>"_s, "outerHTML");
        check(element->localName() == "div"_s, "localName");
    }
    check(document->url().host() == "crt.invalid"_s, "document URL");

    // A Skia surface: ImageBuffer with the unaccelerated (CPU) Skia backend.
    auto buffer = ImageBuffer::create(FloatSize { 64, 32 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified,
        1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    check(!!buffer, "ImageBuffer::create");
    if (buffer) {
        auto& context = buffer->context();
        context.fillRect(FloatRect { 0, 0, 32, 32 }, Color::red);
        context.fillRect(FloatRect { 32, 0, 32, 32 }, Color::green);

        auto pixels = buffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, { 0, 0, 64, 32 });
        check(!!pixels, "getPixelBuffer");
        if (pixels) {
            auto bytes = pixels->bytes();
            auto pixel = [&](int x, int y) { return &bytes[(y * 64 + x) * 4]; }; // B, G, R, A
            check(pixel(8, 8)[2] == 255 && pixel(8, 8)[1] == 0 && pixel(8, 8)[3] == 255, "red rectangle");
            check(pixel(40, 8)[1] >= 128 && pixel(40, 8)[2] == 0 && pixel(40, 8)[3] == 255, "green rectangle");
        }
    }

    // Text: fontconfig -> FreeType -> HarfBuzz -> Skia, through WebCore's font system.
    FontCascadeDescription description;
    description.setOneFamily("DejaVu Sans Mono"_s);
    description.setComputedSize(16);
    description.setSpecifiedSize(16);
    FontCascade font(FontCascadeDescription { description });
    font.update(nullptr);
    TextRun run { StringView { "Hello CRT"_s } };
    float width = font.width(run);
    printf("crtweb_webcore_graph_test: text width %.2f\n", width);
    // DejaVu Sans Mono is monospaced: nine glyphs of 0.602 em at 16 px.
    check(width > 80 && width < 100, "monospaced text width");
    auto textBuffer = ImageBuffer::create(FloatSize { 128, 32 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified,
        1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    check(!!textBuffer, "text ImageBuffer");
    if (textBuffer) {
        auto& context = textBuffer->context();
        context.fillRect(FloatRect { 0, 0, 128, 32 }, Color::white);
        context.setFillColor(Color::black);
        context.drawText(font, run, FloatPoint { 4, 22 });
        auto pixels = textBuffer->getPixelBuffer({ AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, { 0, 0, 128, 32 });
        size_t dark = 0;
        if (pixels) {
            auto bytes = pixels->bytes();
            for (size_t i = 0; i + 3 < bytes.size(); i += 4) {
                if (bytes[i + 1] < 96)
                    ++dark;
            }
        }
        printf("crtweb_webcore_graph_test: %zu dark text pixels\n", dark);
        check(dark > 40, "text was painted");
    }

    if (failures)
        return 1;
    printf("crtweb_webcore_graph_test: ok\n");
    return 0;
}
