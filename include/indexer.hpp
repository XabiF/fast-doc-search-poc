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

using WordHash = u32;
constexpr WordHash InvalidWordHash = UINT32_MAX;

inline constexpr bool IsWordHashInvalid(WordHash word_hash) {
    return word_hash == InvalidWordHash;
}

constexpr std::string_view WordTablesFilePath = "words.wtb";
constexpr std::string_view DocumentTablesFilePath = "documents.dtb";

struct WtbHeader {
    u32 last_allocated_word_hash;
    u32 first_k3c_lookup_table_offsets[WhitelistCharCount*WhitelistCharCount*WhitelistCharCount];
    u32 first_rlist_offset;
    u32 last_rlist_offset;
};
static_assert(sizeof(WtbHeader) == sizeof(u32)*3 + sizeof(u32)*WhitelistCharCount*WhitelistCharCount*WhitelistCharCount);

struct DtbHeader {
    u32 last_doc_table_list_offset;
    u32 first_k3c_occ_table_offsets[WhitelistCharCount*WhitelistCharCount*WhitelistCharCount];
};
static_assert(sizeof(DtbHeader) == sizeof(u32) + sizeof(u32)*WhitelistCharCount*WhitelistCharCount*WhitelistCharCount);

constexpr u32 SegmentStartIndicator = 0x44556677;
constexpr u32 DocumentEndIndicator = 0x00AA11BB;
constexpr u32 InvalidOffset = 0xEEEEEEEE;
constexpr u32 WordTableSize = 10;
constexpr u8 WordTableEndIndicator = 0xFF;
constexpr size_t ReverseLookupTableSize = 10000;

bool IndexAllFiles(std::string_view docs_dir);
bool IndexNewFile(std::string_view doc_path);
