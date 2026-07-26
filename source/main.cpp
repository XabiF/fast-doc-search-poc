/*
    fast-doc-search-poc
    Copyright © 2026 Xabier Fernández

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <cstring>
#include <curses.h>
#include <indexer.hpp>
#include <print>
#include <searcher.hpp>

void ShowSearchCli() {
    SearchContext ctx;
    if(!ctx.Open()) {
        std::println("Unable to open search context! Have you indexed your documents first?");
        return;
    }

    size_t query_offset = 0;
    std::string query;

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, true);
    scrollok(stdscr, true);

    do {
        mvprintw(1, 1, "> %s", query.c_str());

        u32 cur_x = 1;
        u32 cur_y = 5;
        double elapsed_time;
        SearchResult res;
        bool search_ok;
        TIME_FUNCTION_SINGLE(ctx.SearchSingleWord(query, 5, res), elapsed_time, search_ok);
        if(search_ok) {
            u32 total_count = 0;
            for(const auto &doc: res.per_doc) {
                mvprintw(cur_y, cur_x, ":: PDF '%s' in", doc.doc_path.c_str()); cur_y++;
                for(const auto &entry: doc.entries) {
                    total_count++;
                    mvprintw(cur_y, cur_x+3, "(%d) '...%s...'", entry.segment_val, entry.fmt_content.c_str()); cur_y++;
                }
            }
            mvprintw(4, cur_x, "A total of %d results were found!", total_count); cur_y++;
        }
        else {
            mvprintw(4, cur_x, "Word not found!"); cur_y++;
        }
        mvprintw(3, cur_x, "Search took %fs", elapsed_time); cur_y++;

        move(1, 1 + 2 + query_offset);

        const auto ch = getch();

        erase();

        if(ch == KEY_F(10)) {
            break;
        }
        if(ch == KEY_BACKSPACE) {
            if(query_offset >= 1) {
                query.erase(query.begin() + query_offset - 1);
                query_offset--;
            }
        }
        else if(ch == KEY_LEFT) {
            if(query_offset > 0) {
                query_offset--;
            }
        }
        else if(ch == KEY_RIGHT) {
            if(query_offset < query.length()) {
                query_offset++;
            }
        }
        else if(isprint(ch)) {
            query.insert(query.begin() + query_offset, ch);
            query_offset++;
        }
    } while(true);
    
    refresh();
    endwin();
}

int main(int argc, const char *argv[]) {
    if(argc < 2) {
        ShowSearchCli();
    }
    else {
        const auto command = argv[1];
        if(std::strcmp(command, "index") == 0) {
            if(argc < 3) {
                std::println("No param supplied!");
                return 1;    
            }

            // Index PDFs
            const auto docs_dir = argv[2];
            double elapsed_time;
            bool index_ok;
            TIME_FUNCTION_SINGLE(IndexAllFiles(docs_dir), elapsed_time, index_ok);
            if(index_ok) {
                std::println("Indexing finished! Took a total of {:.3f}s", elapsed_time);
            }
            else {
                std::println("Unable to index documents...");
                return 1;
            }
        }
        else {
            std::println("Invalid command '{}'!", command);
            return 1;
        }
    }

    return 0;
}
