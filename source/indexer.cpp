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

#include <indexer.hpp>
#include <print>
#include <string>
#include <string_view>
#include <unordered_map>
#include <set>
#include <filesystem>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>

namespace {

    enum class DocumentType {
        Pdf,
        Plaintext
    };

    bool IdentifyDocument(const std::filesystem::path &path, DocumentType &out_type) {
        const auto extension = path.filename().extension();
        if(extension == ".pdf") {
            out_type = DocumentType::Pdf;
            return true;
        }
        else if(extension == ".txt") {
            out_type = DocumentType::Plaintext;
            return true;
        }
        return false;
    }

    struct WordOccurrence {
            u32 occ_offset;
            u32 segment_val;
        };

        union K3cKey {
        using Value = u32;
        struct {
            char ch1;
            char ch2;
            char ch3;
        } chars;
        Value raw;

        static inline constexpr Value Pack(char ch1, char ch2, char ch3) {
            return (K3cKey{.chars = {ch1, ch2, ch3}}).raw;
        }

        static inline constexpr Value FromString(std::string_view str) {
            return Pack(str[0], str[1], str[2]);
        }
    };
    static_assert(sizeof(K3cKey) == 4);

    struct FullIndexContext {
        using PlainWordMap = std::unordered_map<std::string, WordHash>;
        using K3cWordMap = std::unordered_map<K3cKey::Value, std::vector<PlainWordMap::iterator>>;

        File wtb_file;
        File dtb_file;
        u32 cur_word_hash;
        PlainWordMap plain_word_map;
        K3cWordMap k3c_word_map;
        std::set<char> charset;

        WordHash RegisterWord(const std::string &word) {
            const auto hash_lookup = this->plain_word_map.find(word);
            if(hash_lookup == this->plain_word_map.end()) {
                const auto word_hash = this->cur_word_hash;
                const auto &[new_hash_lookup, _] = this->plain_word_map.insert({ word, word_hash });

                const auto key = K3cKey::FromString(word);
                const auto key_lookup = this->k3c_word_map.find(key);
                if(key_lookup == this->k3c_word_map.end()) {
                    this->k3c_word_map.insert({ key, { new_hash_lookup } });
                }
                else {
                    key_lookup->second.push_back(new_hash_lookup);
                }

                this->cur_word_hash++;
                return word_hash;
            }
            else {
                return hash_lookup->second;
            }
        }

        bool AppendWord(std::string &word) {
            if(CleanWord(word)) {
                const auto word_hash = this->RegisterWord(word);
                if(!IsWordHashInvalid(word_hash)) {
                    TRY(this->dtb_file.Write(word_hash));
                }
            }
            return true;
        }
    };

}

bool IndexPlaintextFile(FullIndexContext &ctx, std::string_view path) {
    MappedFile doc_file;
    TRY(doc_file.Open(path));

    std::string cur_word;
    u32 cur_segment_val = 1;
    TRY(ctx.dtb_file.Write(SegmentStartIndicator));
    TRY(ctx.dtb_file.Write(cur_segment_val));

    size_t cur_offset = 0;
    auto doc_str_buf = reinterpret_cast<char*>(doc_file.addr);
    while(cur_offset < doc_file.size) {
        const auto cur_ch = tolower(doc_str_buf[cur_offset]);
        if(cur_ch == ' ') {
            TRY(ctx.AppendWord(cur_word));
            cur_word.clear();
        }
        else if(cur_ch == '~') {
            cur_segment_val++;
            TRY(ctx.dtb_file.Write(SegmentStartIndicator));
            TRY(ctx.dtb_file.Write(cur_segment_val));
        }
        else if(cur_ch == '\n') {
            if(!cur_word.empty() && (doc_str_buf[cur_offset-1] == '-')) {
                cur_word.pop_back();
            }
            else {
                TRY(ctx.AppendWord(cur_word));
                cur_word.clear();
            }
        }
        else {
            if(WhitelistChars.find(cur_ch) != std::string_view::npos) {
                ctx.charset.insert(cur_ch);
                cur_word += cur_ch;
            }
        }

        cur_offset++;
    }
    TRY(ctx.AppendWord(cur_word));

    return true;
}

bool IndexPdfFile(FullIndexContext &ctx, const std::string &path) {
    const auto pdf_doc = poppler::document::load_from_file(path);
    ON_SCOPE_EXIT([&pdf_doc]() { delete pdf_doc; });

    for(int i = 0; i < pdf_doc->pages(); i++) {
        const auto page = pdf_doc->create_page(i)->text().to_latin1();
        TRY(ctx.dtb_file.Write(SegmentStartIndicator));
        TRY(ctx.dtb_file.Write(i+1));

        std::string cur_word;
        for(u32 j = 0; j < page.length(); j++) {
            const auto cur_ch = tolower(page[j]);
            if(cur_ch == ' ') {
                TRY(ctx.AppendWord(cur_word));
                cur_word.clear();
            }
            else if(cur_ch == '\n') {
                if(!cur_word.empty() && (page[j-1] == '-')) {
                    cur_word.pop_back();
                }
                else {
                    TRY(ctx.AppendWord(cur_word));
                    cur_word.clear();
                }
            }
            else {
                if(WhitelistChars.find(cur_ch) != std::string_view::npos) {
                    ctx.charset.insert(cur_ch);
                    cur_word += cur_ch;
                }
            }
        }
        TRY(ctx.AppendWord(cur_word));
    }

    return true;
}

bool IndexAllFiles(std::string_view docs_dir) {
    FullIndexContext ctx = {
        .cur_word_hash = 0 // Initial, incremental word hash value
    };   

    TRY(ctx.wtb_file.Open(WordTablesFilePath, "wb"));
    TRY(ctx.dtb_file.Open(DocumentTablesFilePath, "wb"));

    // Skip last_doc_offset
    TRY(ctx.dtb_file.Write(InvalidOffset));

    u32 total_doc_count = 0;
    auto cur_doc_offset = ctx.dtb_file.GetOffset();
    for(const auto &doc_entry : std::filesystem::recursive_directory_iterator(docs_dir)) {
        if(!doc_entry.is_regular_file()) {
            continue;
        }

        DocumentType doc_type;
        if(!IdentifyDocument(doc_entry.path(), doc_type)) {
            continue;
        }

        const auto next_doc_offset = ctx.dtb_file.GetOffset();

        if(total_doc_count > 0) { 
            // Write cur doc offset in prev doc offset
            TRY(ctx.dtb_file.Seek(cur_doc_offset, SEEK_SET));
            TRY(ctx.dtb_file.Write<u32>(next_doc_offset - cur_doc_offset));
            
            cur_doc_offset = next_doc_offset;
            TRY(ctx.dtb_file.Seek(cur_doc_offset, SEEK_SET));
        }

        // Next doc offset
        TRY(ctx.dtb_file.Write(InvalidOffset));

        const auto doc_path_str = doc_entry.path().string();
        TRY(ctx.dtb_file.Write<u16>(doc_path_str.length()));
        TRY(ctx.dtb_file.WriteBuffer(doc_path_str.c_str(), doc_path_str.length()));

        switch(doc_type) {
            case DocumentType::Pdf: {
                std::println(":: Indexing PDF document '{}'...", doc_path_str);
                TRY(IndexPdfFile(ctx, doc_path_str));
                break;
            }
            case DocumentType::Plaintext: {
                std::println(":: Indexing plaintext document '{}'...", doc_path_str);
                TRY(IndexPlaintextFile(ctx, doc_path_str));
                break;
            }
        }

        TRY(ctx.dtb_file.Write(DocumentEndIndicator));
        total_doc_count++;
    }

    const auto total_word_count = ctx.plain_word_map.size();
    std::println("-- Identified a total of {} words", total_word_count);
    auto word_offset_buf = std::make_unique<u32[]>(total_word_count);

    TRY(ctx.wtb_file.Seek(sizeof(u32) + WhitelistCharCount*WhitelistCharCount*WhitelistCharCount*sizeof(u32) + sizeof(u32) + sizeof(u32), SEEK_SET));

    for(u32 i1 = 0; i1 < WhitelistCharCount; i1++) {
        for(u32 i2 = 0; i2 < WhitelistCharCount; i2++) {
            for(u32 i3 = 0; i3 < WhitelistCharCount; i3++) {
                const auto key = K3cKey::Pack(WhitelistChars[i1], WhitelistChars[i2], WhitelistChars[i3]);                

                auto cur_lookup_table_offset = ctx.wtb_file.GetOffset();
                const auto lookup_value = ctx.k3c_word_map.find(key);

                if(lookup_value != ctx.k3c_word_map.end()) {
                    // Write first table offset
                    TRY(ctx.wtb_file.Seek(sizeof(u32) + (i1*WhitelistCharCount*WhitelistCharCount + i2*WhitelistCharCount + i3)*sizeof(u32), SEEK_SET));
                    TRY(ctx.wtb_file.Write<u32>(cur_lookup_table_offset));

                    TRY(ctx.wtb_file.Seek(cur_lookup_table_offset, SEEK_SET));

                    // Write dummy next table offset
                    TRY(ctx.wtb_file.Write(InvalidOffset));

                    u32 table_word_count = 0;
                    for(const auto &item: lookup_value->second) {
                        const auto word = item->first;

                        if(table_word_count == WordTableSize) {
                            TRY(ctx.wtb_file.Write(WordTableEndIndicator));
                            
                            const auto new_lookup_table_offset = ctx.wtb_file.GetOffset();
                            TRY(ctx.wtb_file.Seek(cur_lookup_table_offset, SEEK_SET));
                            // Write new next table offset
                            TRY(ctx.wtb_file.Write<u32>(new_lookup_table_offset - cur_lookup_table_offset));

                            cur_lookup_table_offset = new_lookup_table_offset;
                            TRY(ctx.wtb_file.Seek(cur_lookup_table_offset, SEEK_SET));
                            TRY(ctx.wtb_file.Write(InvalidOffset));
                            table_word_count = 0;
                        }

                        // const auto word = item->first;
                        const auto word_hash = item->second;

                        const auto cur_word_offset = ctx.wtb_file.GetOffset();
                        word_offset_buf[word_hash] = cur_word_offset;

                        TRY(ctx.wtb_file.Write<u8>(word.length()));
                        TRY(ctx.wtb_file.WriteBuffer(word.data(), word.length()));
                        TRY(ctx.wtb_file.Write(word_hash));
                        table_word_count++;
                    }

                    // End of last table
                    if(table_word_count > 0) {
                        TRY(ctx.wtb_file.Write(WordTableEndIndicator));
                    }
                }
                else {
                    // Write invalid table offset
                    TRY(ctx.wtb_file.Seek(sizeof(u32) + (i1*WhitelistCharCount*WhitelistCharCount + i2*WhitelistCharCount + i3)*sizeof(u32), SEEK_SET));
                    TRY(ctx.wtb_file.Write(InvalidOffset));
                    TRY(ctx.wtb_file.Seek(cur_lookup_table_offset, SEEK_SET));
                }
            }
        }
    }

    auto cur_reverse_lookup_list_offset = ctx.wtb_file.GetOffset();

    TRY(ctx.wtb_file.Seek(sizeof(u32) + WhitelistCharCount*WhitelistCharCount*WhitelistCharCount*sizeof(u32), SEEK_SET));
    TRY(ctx.wtb_file.Write<u32>(cur_reverse_lookup_list_offset));
    TRY(ctx.wtb_file.Write<u32>(cur_reverse_lookup_list_offset));

    TRY(ctx.wtb_file.Seek(cur_reverse_lookup_list_offset, SEEK_SET));
    TRY(ctx.wtb_file.Write(InvalidOffset));
    TRY(ctx.wtb_file.Write<u32>(0));
    const u32 last_word_hash = std::min(total_word_count, ReverseLookupTableSize);
    TRY(ctx.wtb_file.Write(last_word_hash));
    TRY(ctx.wtb_file.WriteBuffer(word_offset_buf.get(), sizeof(WordHash)*last_word_hash));

    u32 cur_word_hash = last_word_hash;
    while(cur_word_hash < total_word_count) {
        const auto new_reverse_lookup_list_offset = ctx.wtb_file.GetOffset();
        
        TRY(ctx.wtb_file.Seek(cur_reverse_lookup_list_offset, SEEK_SET));
        TRY(ctx.wtb_file.Write<u32>(new_reverse_lookup_list_offset - cur_reverse_lookup_list_offset));

        cur_reverse_lookup_list_offset = new_reverse_lookup_list_offset;
        
        TRY(ctx.wtb_file.Seek(cur_reverse_lookup_list_offset, SEEK_SET));
        TRY(ctx.wtb_file.Write(InvalidOffset));
        TRY(ctx.wtb_file.Write(cur_word_hash));
        const u32 last_word_hash = std::min(total_word_count, cur_word_hash + ReverseLookupTableSize);
        const auto hash_count = last_word_hash - cur_word_hash;
        TRY(ctx.wtb_file.Write(last_word_hash));
        TRY(ctx.wtb_file.WriteBuffer(word_offset_buf.get() + cur_word_hash, sizeof(WordHash)*hash_count));

        cur_word_hash = last_word_hash;
    }

    return true;
}
