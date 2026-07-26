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
#include <base.hpp>
#include <cstdio>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

struct File {
    FILE *f;

    constexpr File() : f(nullptr) {}
    
    inline ~File() {
        this->Close();
    }

    inline bool Open(std::string_view path, std::string_view mode) {
        this->f = fopen(path.data(), mode.data());
        return this->f != nullptr;
    }

    template<typename T>
    inline bool Write(const T t) {
        return fwrite(&t, sizeof(t), 1, this->f) == 1;
    }

    inline bool WriteBuffer(const void *buf, size_t buf_size) {
        return fwrite(buf, buf_size, 1, this->f) == 1;
    }

    template<typename T>
    inline bool Read(T &out_t) {
        return fread(std::addressof(out_t), sizeof(out_t), 1, this->f) == 1;
    }

    inline bool ReadBuffer(void *buf, size_t buf_size) {
        return fread(buf, buf_size, 1, this->f) == 1;
    }

    inline bool Seek(off_t offset, int whence) {
        return fseek(this->f, offset, whence) == 0;
    }

    inline size_t GetOffset() {
        return ftell(this->f);
    }

    void Close() {
        if(this->f != nullptr) {
            fclose(this->f);
            this->f = nullptr;
        }
    }
};

struct MappedFile {
    u8 *addr;
    size_t size;

    #define MAP_FAILED_U8 reinterpret_cast<u8*>(MAP_FAILED)

    constexpr MappedFile() : addr(MAP_FAILED_U8), size(0) {}
    
    inline ~MappedFile() {
        this->Close();
    }

    inline bool Open(std::string_view path) {
        const auto fd = open(path.data(), O_RDONLY);
        TRY(fd != -1);

        struct stat st;
        TRY(fstat(fd, &st) == 0);

        this->size = st.st_size;
        this->addr = reinterpret_cast<u8*>(mmap(nullptr, this->size, PROT_READ, MAP_SHARED, fd, 0));
        return this->addr != MAP_FAILED;
    }

    inline void Close() {
        if(this->addr != MAP_FAILED) {
            munmap(this->addr, this->size);
            this->addr = MAP_FAILED_U8;
        }
    }
};

constexpr auto EmptyPseudoChar = '*';

constexpr std::string_view WhitelistChars = "abcdefghijklmnopqrstuvwxyz0123456789-:./\\*";
constexpr size_t WhitelistCharCount = WhitelistChars.size();

constexpr std::string_view WhitelistCharsNoEmpty = "abcdefghijklmnopqrstuvwxyz0123456789-:./\\";

constexpr std::string_view WhitelistCharsNoBorders = "-:./\\*";
constexpr size_t WhitelistCharsNoBordersCount = WhitelistCharsNoBorders.size();

bool CleanWord(std::string &word);
bool ProcessWord(std::string_view word, std::string &out_word);
