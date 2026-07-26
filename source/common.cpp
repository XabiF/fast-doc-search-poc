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

#include <common.hpp>

bool CleanWord(std::string &word) {
    if(word.empty()) {
        return false;
    }

    bool first_part_done = false;
    bool last_part_done = false;
    while(!first_part_done or !last_part_done) {
        if(!first_part_done) {
            first_part_done = WhitelistCharsNoBorders.find(word.front()) == std::string_view::npos;
            if(!first_part_done) {
                word.erase(word.begin());
            }
        }
        if(word.empty()) {
            return false;
        }
        
        if(!last_part_done) {
            last_part_done = WhitelistCharsNoBorders.find(word.back()) == std::string_view::npos;
            if(!last_part_done) {
                word.pop_back();
            }
        }
        if(word.empty()) {
            return false;
        }
    }

    if(word.size() >= UINT8_MAX) {
        return false;
    }
    while(word.size() < 3) {
        word += EmptyPseudoChar;
    }

    return true;
}

bool ProcessWord(std::string_view word, std::string &out_word) {
    if(word.empty()) {
        return false;
    }

    out_word.clear();
    for(const auto ch: word) {
        if(WhitelistCharsNoEmpty.find(ch) != std::string_view::npos) {
            out_word += ch;
        }
    }
    return CleanWord(out_word);
}
