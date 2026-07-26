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

#pragma once
#include <common.hpp>
#include <indexer.hpp>

struct DocumentSearchEntry {
    u32 segment_val;
    std::string fmt_content;
};

struct DocumentSearchResult {
    std::string doc_path;
    std::vector<DocumentSearchEntry> entries;
};

struct SearchResult {
    std::vector<DocumentSearchResult> per_doc;
};

class SearchContext {
    private:
        MappedFile wtb_file;
        WtbHeader *wtb_header;
        MappedFile dtb_file;
        DtbHeader *dtb_header;

        bool GetSurroundingWords(u32 word_offset, u32 surr_word_count, std::vector<WordHash> &prior_word_hashes, std::vector<WordHash> &post_word_hashes);

    public:
        constexpr SearchContext() : wtb_file(), dtb_file() {}

        bool Open();

        bool LookupWordHashK3cOffset(std::string_view word, u32 &out_word_hash, u32 &out_k3c_offset, std::string &out_processed_word);
        
        inline bool LookupWordHash(std::string_view word, u32 &out_word_hash) {
            u32 dummy_offset;
            std::string dummy_word;
            return this->LookupWordHashK3cOffset(word, out_word_hash, dummy_offset, dummy_word);
        }
        
        bool LookupWord(u32 word_hash, std::string &out_word);

        inline bool ExistsWord(std::string_view word) {
            u32 dummy_hash;
            return this->LookupWordHash(word, dummy_hash);
        }

        bool SearchSingleWord(std::string_view word, u32 surr_word_count, SearchResult &out_res);
};
