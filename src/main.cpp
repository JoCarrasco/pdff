#include <iostream>
#include <SDL2/SDL.h>

extern "C" {

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

}
// Global or Struct to keep track of state
int current_page_num = 0;
int total_pages = 0;

SDL_Texture* render_page_to_texture(fz_context *ctx, fz_document *doc, SDL_Renderer *renderer, int page_num) {
    fz_page *page = fz_load_page(ctx, doc, page_num);

    // Zooming in slightly for readability (2.0 = 144 DPI)
    fz_matrix ctm = fz_scale(2.0, 2.0);
    fz_pixmap *pix = fz_new_pixmap_from_page(ctx, page, ctm, fz_device_rgb(ctx), 0);

    SDL_Texture *tex = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, pix->w, pix->h);

    SDL_UpdateTexture(tex, NULL, pix->samples, pix->w * 3);

    fz_drop_pixmap(ctx, pix);
    fz_drop_page(ctx, page);
    return tex;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;

    // Initialize MuPDF
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(ctx);
    fz_document *doc = fz_open_document(ctx, argv[1]);
    total_pages = fz_count_pages(ctx, doc);

    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);

    // We'll create a window. Note: Real readers handle window resizing,
    // but we'll start fixed based on the first page render.
    SDL_Window *window = SDL_CreateWindow("MuPDF Reader", 100, 100, 800, 1000, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Initial render
    SDL_Texture *current_tex = render_page_to_texture(ctx, doc, renderer, current_page_num);

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                bool changed = false;
                if (event.key.keysym.sym == SDLK_RIGHT && current_page_num < total_pages - 1) {
                    current_page_num++;
                    changed = true;
                }
                else if (event.key.keysym.sym == SDLK_LEFT && current_page_num > 0) {
                    current_page_num--;
                    changed = true;
                }

                if (changed) {
                    SDL_DestroyTexture(current_tex); // Clean up old page memory!
                    current_tex = render_page_to_texture(ctx, doc, renderer, current_page_num);
                    std::cout << "Page: " << current_page_num + 1 << "/" << total_pages << std::endl;
                }
            }
        }

        SDL_RenderClear(renderer);

        // Draw the page centered in the window
        SDL_RenderCopy(renderer, current_tex, NULL, NULL);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(current_tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
    SDL_Quit();

    return 0;
}