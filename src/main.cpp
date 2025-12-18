#include <iostream>
#include <SDL2/SDL.h>

extern "C" {

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

}
// Global or Struct to keep track of state
bool needs_redraw = true;
int current_page_num = 0;
int total_pages = 0;

// Sizing
float page_width;
float page_height;
float aspect_ratio;

SDL_Rect calculate_dest_rect(int win_w, int win_h, int tex_w, int tex_h) {
    SDL_Rect dest;
    float tex_aspect = (float)tex_w / (float)tex_h;
    float win_aspect = (float)win_w / (float)win_h;

    if (win_aspect > tex_aspect) {
        // Window is wider than PDF (Pillarboxing)
        dest.h = win_h;
        dest.w = (int)(win_h * tex_aspect);
        dest.x = (win_w - dest.w) / 2;
        dest.y = 0;
    } else {
        // Window is taller than PDF (Letterboxing)
        dest.w = win_w;
        dest.h = (int)(win_w / tex_aspect);
        dest.x = 0;
        dest.y = (win_h - dest.h) / 2;
    }
    return dest;
}

void render_with_aspect_ratio(SDL_Renderer *renderer, SDL_Texture *tex, const int &window_w, const int &window_h, const float &asp_ratio) {
    SDL_Rect dest_rect;

    if (static_cast<float>(window_w) / window_h > asp_ratio) {
        // Window is wider than PDF - Pillarboxing (black bars on sides)
        dest_rect.h = window_h;
        dest_rect.w = static_cast<int>(window_h * asp_ratio);
        dest_rect.x = (window_w - dest_rect.w) / 2;
        dest_rect.y = 0;
    } else {
        // Window is taller than PDF - Letterboxing (black bars top/bottom)
        dest_rect.w = window_w;
        dest_rect.h = static_cast<int>(window_w / asp_ratio);
        dest_rect.x = 0;
        dest_rect.y = (window_h - dest_rect.h) / 2;
    }

    SDL_RenderCopy(renderer, tex, nullptr, &dest_rect);
}


SDL_Texture* render_page_to_texture(fz_context *ctx, fz_document *doc, SDL_Renderer *renderer, const int &page_num) {
    fz_page *page = fz_load_page(ctx, doc, page_num);

    // 1. Maximize Anti-Aliasing
    fz_set_aa_level(ctx, 8);

    // 2. High Scale (3.0 is the "sweet spot" for 1080p-4k screens)
    float scale = 3.0f;
    fz_matrix ctm = fz_scale(scale, scale);

    fz_rect rect = fz_bound_page(ctx, page);
    fz_irect bbox = fz_round_rect(fz_transform_rect(rect, ctm));

    // 3. Create Pixmap (0 = No alpha, results in cleaner text contrast)
    fz_pixmap *pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, NULL, 0);
    fz_clear_pixmap_with_value(ctx, pix, 255);

    // 4. Render with Draw Device
    fz_device *dev = fz_new_draw_device(ctx, ctm, pix);
    fz_run_page(ctx, page, dev, fz_identity, NULL);
    fz_close_device(ctx, dev);
    fz_drop_device(ctx, dev);

    // 5. Use ARGB for better subpixel compatibility with modern GPUs
    SDL_Texture *tex = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, pix->w, pix->h);

    SDL_UpdateTexture(tex, NULL, pix->samples, pix->stride);

    fz_drop_pixmap(ctx, pix);
    fz_drop_page(ctx, page);

    return tex;
}

int main(const int argc, const char **argv) {
    if (argc < 2) return 1;

    // Initialize MuPDF
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(ctx);
    fz_document *doc = fz_open_document(ctx, argv[1]);
    total_pages = fz_count_pages(ctx, doc);

    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);

    // We'll create a window. Note: Real readers handle window resizing,
    // but we'll start fixed based on the first page render.
    SDL_Window *window = SDL_CreateWindow("MuPDF Reader", 100, 100, 800, 1000, SDL_WINDOW_RESIZABLE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE); // Keeps text sharp
    // Initial render
    SDL_Texture *current_tex = render_page_to_texture(ctx, doc, renderer, current_page_num);
    Uint32 resize_timer = 0;
    bool is_resizing = false;
    bool running = true;
    SDL_Event event;
    while (running) {
        if (SDL_WaitEventTimeout(&event, 10)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    is_resizing = true;
                    needs_redraw = true;
                    // Set timer to 300ms in the future
                    resize_timer = SDL_GetTicks() + 300;
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_RIGHT && current_page_num < total_pages - 1) {
                    current_page_num++;
                    SDL_DestroyTexture(current_tex);
                    current_tex = render_page_to_texture(ctx, doc, renderer, current_page_num);
                    needs_redraw = true;
                } else if (event.key.keysym.sym == SDLK_LEFT && current_page_num > 0) {
                    current_page_num--;
                    SDL_DestroyTexture(current_tex);
                    current_tex = render_page_to_texture(ctx, doc, renderer, current_page_num);
                    needs_redraw = true;
                } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    needs_redraw = true;
                }
            }
        }

        // Check if user stopped resizing
        if (is_resizing && SDL_TICKS_PASSED(SDL_GetTicks(), resize_timer)) {
            // Now that they stopped, re-render the PDF for the new size
            SDL_DestroyTexture(current_tex);
            current_tex = render_page_to_texture(ctx, doc, renderer, current_page_num);
            is_resizing = false;
            needs_redraw = true;
        }

        if (needs_redraw) {
            int ww, wh, tw, th;
            SDL_GetWindowSize(window, &ww, &wh);
            SDL_QueryTexture(current_tex, nullptr, nullptr, &tw, &th);

            // Calculate the centered, non-distorted rectangle
            SDL_Rect dest = calculate_dest_rect(ww, wh, tw, th);

            // Background color (Dark Gray)
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
            SDL_RenderClear(renderer);

            // Draw the PDF at its proper ratio
            SDL_RenderCopy(renderer, current_tex, nullptr, &dest);

            SDL_RenderPresent(renderer);
            needs_redraw = false;
        }
    }

    SDL_DestroyTexture(current_tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
    SDL_Quit();

    return 0;
}