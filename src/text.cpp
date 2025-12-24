#include "text.h"
extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/fitz/structured-text.h>
}

fz_stext_page* TextUtils::get_page_text(fz_context *ctx, fz_page *page) {
    return fz_new_stext_page_from_page(ctx, page, nullptr);
}
