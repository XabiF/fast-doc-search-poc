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
#include <searcher.hpp>

bool SearchContext::Open() {
    TRY(this->wtb_file.Open(WordTablesFilePath.data()));
    this->wtb_header = reinterpret_cast<WtbHeader*>(this->wtb_file.addr);

    TRY(this->dtb_file.Open(DocumentTablesFilePath.data()));
    this->dtb_header = reinterpret_cast<DtbHeader*>(this->dtb_file.addr);

    return true;
}

bool SearchContext::LookupWordHashK3cOffset(std::string_view word, u32 &out_word_hash, u32 &out_k3c_offset, std::string &out_processed_word) {
    TRY(ProcessWord(word, out_processed_word));

    const auto i1 = WhitelistChars.find(out_processed_word[0]);
    TRY(i1 != std::string_view::npos);
    const auto i2 = WhitelistChars.find(out_processed_word[1]);
    TRY(i2 != std::string_view::npos);
    const auto i3 = WhitelistChars.find(out_processed_word[2]);
    TRY(i3 != std::string_view::npos);

    auto cur_lookup_table_offset = this->wtb_header->first_k3c_lookup_table_offsets[i1*WhitelistCharCount*WhitelistCharCount + i2*WhitelistCharCount + i3];
    if(cur_lookup_table_offset == InvalidOffset) {
        return false;
    }

    size_t cur_offset = cur_lookup_table_offset;
    while(true) {
        const auto next_lookup_table_rel_offset = *reinterpret_cast<u32*>(this->wtb_file.addr + cur_offset); cur_offset += sizeof(u32);
        while(true) {
            const auto next8 = *reinterpret_cast<u8*>(this->wtb_file.addr + cur_offset); cur_offset++;
            if(next8 == WordTableEndIndicator) {
                break;
            }

            const auto word_len = next8;
            auto word_ptr = reinterpret_cast<char*>(this->wtb_file.addr + cur_offset); cur_offset += word_len;
            std::string read_word(word_ptr, word_ptr + word_len);

            const auto word_hash = *reinterpret_cast<u32*>(this->wtb_file.addr + cur_offset); cur_offset += sizeof(u32);

            if(out_processed_word == read_word) {
                out_word_hash = word_hash;
                return true;
            }
        }

        if(next_lookup_table_rel_offset == InvalidOffset) {
            break;
        }
        cur_lookup_table_offset += next_lookup_table_rel_offset;
    }

    return false;
}

struct ReverseLookupListHeader {
    u32 next_rlist_rel_offset;
    u32 word_hash_start;
    u32 word_hash_end;
};

bool SearchContext::LookupWord(u32 word_hash, std::string &out_word) {
    auto cur_rlist_offset = this->wtb_header->first_rlist_offset;
    while(true) {
        const auto header = reinterpret_cast<ReverseLookupListHeader*>(this->wtb_file.addr + cur_rlist_offset);
        if((word_hash >= header->word_hash_start) && (word_hash < header->word_hash_end)) {
            auto cur_offset = cur_rlist_offset + sizeof(ReverseLookupListHeader) + (word_hash - header->word_hash_start)*sizeof(u32);

            const auto word_offset = *reinterpret_cast<u32*>(this->wtb_file.addr + cur_offset); cur_offset += sizeof(u32);

            const auto word_len = *reinterpret_cast<u8*>(this->wtb_file.addr + word_offset);
            auto word_ptr = reinterpret_cast<char*>(this->wtb_file.addr + word_offset + 1);
            out_word.assign(word_ptr, word_ptr + word_len);

            while(out_word.back() == EmptyPseudoChar) {
                out_word.pop_back();
            }
            return true;
        }

        if(header->next_rlist_rel_offset == InvalidOffset) {
            break;
        }
        cur_rlist_offset += header->next_rlist_rel_offset;
    }

    return false;
}

bool SearchContext::GetSurroundingWords(u32 word_offset, u32 surr_word_count, std::vector<WordHash> &prior_word_hashes, std::vector<WordHash> &post_word_hashes) {
    for(u32 i = 0; i < surr_word_count; i++) {
        const auto prior32 = *reinterpret_cast<u32*>(this->dtb_file.addr + word_offset - (i+1)*sizeof(u32));
        if(prior32 == SegmentStartIndicator) {
            prior_word_hashes.pop_back();
            break;
        }
        const auto prior_word_hash = prior32;
        prior_word_hashes.push_back(prior_word_hash);
    }

    for(u32 i = 0; i < surr_word_count; i++) {
        const auto post32 = *reinterpret_cast<u32*>(this->dtb_file.addr + word_offset + (i+1)*sizeof(u32));
        if((post32 == SegmentStartIndicator) || (post32 == DocumentEndIndicator)) {
            break;
        }
        const auto post_word_hash = post32;
        post_word_hashes.push_back(post_word_hash);
    }

    return true;
}

bool SearchContext::SearchSingleWord(std::string_view word, u32 surr_word_count, SearchResult &out_res) {
    u32 word_hash;
    u32 k3c_offset;
    std::string processed_word;
    TRY(this->LookupWordHashK3cOffset(word, word_hash, k3c_offset, processed_word));

    auto cur_doc_offset = sizeof(u32);
    while(true) {
        auto cur_offset = cur_doc_offset;

        DocumentSearchResult doc_res = {};

        const auto next_doc_rel_offset = *reinterpret_cast<u32*>(this->dtb_file.addr + cur_offset); cur_offset += sizeof(u32);
        const auto doc_path_len = *reinterpret_cast<u16*>(this->dtb_file.addr + cur_offset); cur_offset += sizeof(u16);

        auto doc_path_ptr = reinterpret_cast<char*>(this->dtb_file.addr + cur_offset); cur_offset += doc_path_len;
        doc_res.doc_path.assign(doc_path_ptr, doc_path_ptr + doc_path_len);

        u32 cur_segment_val = 0xFF;
        while(true) {
            const auto next32 = *reinterpret_cast<u32*>(this->dtb_file.addr + cur_offset); cur_offset += sizeof(u32);
            if(next32 == SegmentStartIndicator) {
                cur_segment_val = *reinterpret_cast<u32*>(this->dtb_file.addr + cur_offset); cur_offset += sizeof(u32);
            }
            else if(next32 == DocumentEndIndicator) {
                break;
            }
            else {
                const auto cur_word_hash = next32;
                if(cur_word_hash == word_hash) {
                    const auto word_offset = cur_offset - sizeof(WordHash);
                    
                    auto &entry = doc_res.entries.emplace_back();
                    entry.segment_val = cur_segment_val;

                    std::vector<u32> prior_word_hashes;
                    std::vector<u32> post_word_hashes;
                    TRY(this->GetSurroundingWords(word_offset, surr_word_count, prior_word_hashes, post_word_hashes));

                    std::stringstream strm;
                    for(auto i = prior_word_hashes.rbegin(); i != prior_word_hashes.rend(); i++) {
                        std::string prior_word;
                        if(!this->LookupWord(*i, prior_word)) {
                            continue;
                        }
                        strm << prior_word << " ";
                    }
                    strm << "{" << word << "}"; // Note: format specially chosen for CLI printing, could be otherwise adapted
                    for(const auto post_word_hash: post_word_hashes) {
                        std::string post_word;
                        if(!this->LookupWord(post_word_hash, post_word)) {
                            break;
                        }
                        strm << " " << post_word;
                    }
                    entry.fmt_content = strm.str();
                }
            }
        }

        if(!doc_res.entries.empty()) {
            out_res.per_doc.push_back(std::move(doc_res));
        }

        if(next_doc_rel_offset == InvalidOffset) {
            break;
        }
        cur_doc_offset += next_doc_rel_offset;
    }

    return true;
}
