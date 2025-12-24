#ifndef PDFF_TEXT_H
#define PDFF_TEXT_H
#include <string>
#include <vector>

extern "C" {
    #include <mupdf/fitz.h>

}

struct TextGroup {
    std::string text;
    fz_rect bbox;
    std::string font_name;
    float font_size;
    uint32_t color; // argb from your struct
};

class TextUtils {

static fz_stext_page* get_page_text( fz_context *ctx,  fz_page *page);
};


#endif //PDFF_TEXT_H