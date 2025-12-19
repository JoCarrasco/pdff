#ifndef PDFF_CORE_H
#define PDFF_CORE_H

#include <SDL2/SDL.h>
extern "C" {
    #include <mupdf/fitz.h>
}

class PDFCore {
    public:
        void open(const std::string &file_path);
        int run();
    private:
        Uint32 resize_timer = 0;
        fz_document *doc = nullptr;
        SDL_Window *window = SDL_CreateWindow("MuPDF Reader", 100, 100, 800, 1000, SDL_WINDOW_RESIZABLE);
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *current_tex = nullptr;
        fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);

        bool is_resizing = false;
        bool running = true;
        bool needs_redraw = true;


        float page_width{};
        float page_height{};
        float aspect_ratio{};
        SDL_Texture* render_page_to_texture(const int &page_num);
        static SDL_Rect calculate_dest_rect(const int &win_w, const int &win_h, const int &tex_w, int &tex_h);
};


#endif //PDFF_CORE_H