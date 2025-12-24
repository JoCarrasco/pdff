#include <string>
#include <iostream>
#include "core.h"

int PDFCore::run() {
    SDL_Event event{};
    const int total_pages = fz_count_pages(ctx, doc);

     while (running) {
         int ww, wh, tw, th;
         SDL_GetWindowSize(window, &ww, &wh);
         SDL_QueryTexture(current_tex, nullptr, nullptr, &tw, &th);
         SDL_Rect dest = calculate_dest_rect(ww, wh, tw, th);

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
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    // Get current destination rect and page for conversion
                    SDL_GetWindowSize(window, &ww, &wh);
                    SDL_QueryTexture(current_tex, nullptr, nullptr, &tw, &th);
                    dest = calculate_dest_rect(ww, wh, tw, th);
                    fz_page *page = fz_load_page(ctx, doc, static_cast<int>(current_page));
                    sel_start_pt = screen_to_pdf(mx, my, dest, page);
                    sel_end_pt = sel_start_pt;
                    is_selecting = true;
                    fz_drop_page(ctx, page);
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                if (is_selecting) {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);

                    SDL_GetWindowSize(window, &ww, &wh);
                    SDL_QueryTexture(current_tex, nullptr, nullptr, &tw, &th);
                    dest = calculate_dest_rect(ww, wh, tw, th);

                    fz_page *page = fz_load_page(ctx, doc, static_cast<int>(current_page));
                    sel_end_pt = screen_to_pdf(mx, my, dest, page);
                    needs_redraw = true; // Trigger redraw to show the blue highlight
                    fz_drop_page(ctx, page);
                }
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    is_selecting = false;
                }
            }


            else if (event.type == SDL_KEYDOWN) {
                const bool ctrl_pressed = (SDL_GetModState() & KMOD_CTRL);

                if (ctrl_pressed && event.key.keysym.sym == SDLK_c) {
                    copy_selection_to_clipboard();
                }

                if (event.key.keysym.sym == SDLK_RIGHT && current_page < total_pages - 1) {
                    current_page++;

                    // --- RESET SELECTION ---
                    is_selecting = false;
                    sel_start_pt = {0, 0};
                    sel_end_pt = {0, 0};
                    // -----------------------

                    SDL_DestroyTexture(current_tex);
                    current_tex = render_page_to_texture(static_cast<int>(current_page));
                    needs_redraw = true;
                } else if (event.key.keysym.sym == SDLK_LEFT && current_page > 0) {
                    current_page--;

                    // --- RESET SELECTION ---
                    is_selecting = false;
                    sel_start_pt = {0, 0};
                    sel_end_pt = {0, 0};
                    // -----------------------

                    SDL_DestroyTexture(current_tex);
                    current_tex = render_page_to_texture(static_cast<int>(current_page));
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
            current_tex = render_page_to_texture(static_cast<int>(current_page));
            is_resizing = false;
            needs_redraw = true;
        }

        if (needs_redraw) {
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
            SDL_RenderClear(renderer);

            // Draw PDF
            SDL_RenderCopy(renderer, current_tex, nullptr, &dest);

            // --- DRAW SELECTION HIGHLIGHT ---
            if (is_selecting || (sel_start_pt.x != sel_end_pt.x)) {
                render_selection(dest, static_cast<int>(current_page));
            }

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


void PDFCore::open(const std::string &file_path) {
      fz_register_document_handlers(ctx);
      doc = fz_open_document(ctx, file_path.c_str());
      SDL_Init(SDL_INIT_VIDEO);
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
      SDL_RenderSetIntegerScale(renderer, SDL_TRUE); // Keeps text sharp
      // Initial render
      current_tex = render_page_to_texture(0);
      resize_timer = 0;
      is_resizing = false;
      running = true;
}

SDL_Rect PDFCore::calculate_dest_rect(const int &win_w, const int &win_h, const int &tex_w, const int &tex_h) {
    SDL_Rect dest;
        const float tex_aspect = static_cast<float>(tex_w) / static_cast<float>(tex_h);
        const float win_aspect = static_cast<float>(win_w) / static_cast<float>(win_h);

        if (win_aspect > tex_aspect) {
            // Window is wider than PDF (Pillarboxing)
            dest.h = win_h;
            dest.w = static_cast<int>(floor(win_h) * tex_aspect);
            dest.x = (win_w - dest.w) / 2;
            dest.y = 0;
        } else {
            // Window is taller than PDF (Letterboxing)
            dest.w = win_w;
            dest.h = static_cast<int>(floor(win_w) / tex_aspect);
            dest.x = 0;
            dest.y = (win_h - dest.h) / 2;
        }
        return dest;
}

SDL_Texture* PDFCore::render_page_to_texture(const int &page_num) {
    if (current_stext) {
        fz_drop_stext_page(ctx, current_stext);
    }
    fz_page *page = fz_load_page(ctx, doc, page_num);
    current_stext = fz_new_stext_page_from_page(ctx, page, nullptr);


  // 1. Maximize Anti-Aliasing
  fz_set_aa_level(ctx, 8);

  // 2. High Scale (3.0 is the "sweet spot" for 1080p-4k screens)
  constexpr float scale = 3.0f;
  const fz_matrix ctm = fz_scale(scale, scale);

  const fz_rect rect = fz_bound_page(ctx, page);
  const fz_irect bbox = fz_round_rect(fz_transform_rect(rect, ctm));

  // 3. Create Pixmap (0 = No alpha, results in cleaner text contrast)
  fz_pixmap *pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, nullptr, 0);
  fz_clear_pixmap_with_value(ctx, pix, 255);

  // 4. Render with Draw Device
  fz_device *dev = fz_new_draw_device(ctx, ctm, pix);
  fz_run_page(ctx, page, dev, fz_identity, nullptr);
  fz_close_device(ctx, dev);
  fz_drop_device(ctx, dev);

  // 5. Use ARGB for better subpixel compatibility with modern GPUs
  SDL_Texture *tex = SDL_CreateTexture(renderer,
      SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, pix->w, pix->h);

  SDL_UpdateTexture(tex, nullptr, pix->samples, floor(pix->stride));

  fz_drop_pixmap(ctx, pix);
  fz_drop_page(ctx, page);

  return tex;
}

fz_point PDFCore::screen_to_pdf(const int mx, const int my, const SDL_Rect& dest, fz_page* page) {
    const fz_rect &page_rect = fz_bound_page(ctx, page);
    const float pw = page_rect.x1 - page_rect.x0;
    const float ph = page_rect.y1 - page_rect.y0;

    // Remove the letterbox/pillarbox offset and scale back to PDF points
    const float pdf_x = (static_cast<float>(mx) - dest.x) * (pw / static_cast<float>(dest.w));
    const float pdf_y = (static_cast<float>(my) - dest.y) * (ph / static_cast<float>(dest.h));

    return {pdf_x, pdf_y};
}

void PDFCore::render_selection(const SDL_Rect& dest, const int page_num) {
    fz_page *page = fz_load_page(ctx, doc, page_num);
    fz_stext_page *stext = fz_new_stext_page_from_page(ctx, page, nullptr);

    fz_quad quads[500];
    const int n = fz_highlight_selection(ctx, stext, sel_start_pt, sel_end_pt, quads, 500);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 120, 215, 100); // Highlight color

    const fz_rect p_rect = fz_bound_page(ctx, page);
    const float pw = p_rect.x1 - p_rect.x0;
    const float ph = p_rect.y1 - p_rect.y0;

    for (int i = 0; i < n; i++) {
        SDL_Rect r;
        r.x = dest.x + (quads[i].ul.x * (dest.w / pw));
        r.y = dest.y + (quads[i].ul.y * (dest.h / ph));
        r.w = (quads[i].ur.x - quads[i].ul.x) * (dest.w / pw);
        r.h = (quads[i].ll.y - quads[i].ul.y) * (dest.h / ph);
        SDL_RenderFillRect(renderer, &r);
    }

    fz_drop_stext_page(ctx, stext);
}

void PDFCore::copy_selection_to_clipboard() {
    // 1. Safety check: make sure points aren't identical
    if (sel_start_pt.x == sel_end_pt.x && sel_start_pt.y == sel_end_pt.y) return;

    // 2. Load the page and text structure
    fz_page *page = fz_load_page(ctx, doc, static_cast<int>(current_page));
    fz_stext_page *stext = fz_new_stext_page_from_page(ctx, page, nullptr);

    // 3. Extract the text (MuPDF returns a heap-allocated UTF-8 string)
    // Note: 0 = non-copy-permit (usually ignored), 1 = crlf (Windows style)
    char *selected_text = fz_copy_selection(ctx, stext, sel_start_pt, sel_end_pt, 0);

    if (selected_text) {
        // 4. Send to Ubuntu/System Clipboard
        if (SDL_SetClipboardText(selected_text) != 0) {
            std::cerr << "SDL Clipboard Error: " << SDL_GetError() << std::endl;
        } else {
            std::cout << "Text copied to clipboard!" << std::endl;
        }

        // 5. Cleanup MuPDF allocated string
        fz_free(ctx, selected_text);
    }

    fz_drop_stext_page(ctx, stext);
}