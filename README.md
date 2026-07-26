# Fast document searching Proof-of-Concept

- [Fast document searching Proof-of-Concept](#fast-document-searching-proof-of-concept)
  - [Usage](#usage)
  - [Indexed data formats](#indexed-data-formats)
    - [Document table format](#document-table-format)
    - [Word table format](#word-table-format)

## Usage

The PoC may be compiled with `build.sh` or `debug.sh`. You might need to make minor changes to the compiler or other parts of the build command depending on your system, but I guess it should work out of the box in Linux systems.

- First, index your documents:

```bash
./docsearch index <dir-with-documents>
```

   Note that the PoC scans the directory recursively, and auto-detects PDF or plaintext files based on their extension (either `.pdf` or `.txt`).

   The indexed files `documents.dtb` and `words.wtb` should have been generated in the current working directory.

You may now open the interactive search TUI by running the PoC with no command-line arguments:

```bash
./docsearch
```

## Indexed data formats

The indexing process generates a *document table* file (`documents.dtb`) containing all the preprocessed and indexed document word contents, as well as a *word table* file (`words.wtb`) containing the mapping between words and 32-bit unique word hashes.

In the chosen design, word hashes are simply incremental integers assigned to each new encountered word in the indexing routine.

### Document table format

The format can be qualitatively explained with the following pseudocode:

```cpp
u32 last_doc_offset;
LinkedList<Document> docs...;
```

The structure contains the following blocks:

```cpp
struct Document {
    u32 next_doc_rel_offset; // or fixed 0xEEEEEEEE if there is none
    u16 path_len;
    u8 path[path_len];
    DocumentSegment segments[...];
    u32 doc_end_mark; // fixed 0x00AA11BB magic value
}

struct DocumentSegment {
    u32 doc_segment_start; // fixed 0x44556677 magic value
    u32 segment_val;
    u32 word_hashes[...];
}
```

Each document is encoded as a `Document` block, which are stored in a linked-list-like fashion. Each `Document` begins with a relative offset to the next document entry, in order to simplify jumping between documents.

The document contents are split into so-called "segments", as a way to contextualize the approximate location of words. For PDF documents, each segment is a PDF page (the `segment_val` value corresponds to the page number), while for plaintext files, each segment could be associated with a line.

Magic values are used to indicate the start of a new segment or the ending of the document.

### Word table format

The format can be qualitatively explained with the following pseudocode:

```cpp
u32 last_allocated_word_hash;
u32 first_k3c_lookup_table_offsets[W*W*W]; // 'W' is the number of whitelisted characters that preprocessed words can have
u32 first_rlist_offset;
u32 last_rlist_offset;

LinkedList<K3cLookupTable> tables[W*W*W]...;
LinkedList<ReverseLookupList> rlists...;
```

The structure contains the following blocks:

```cpp
struct K3cLookupTable {
    u32 next_lookup_table_rel_offset; // or fixed 0xEEEEEEEE if there is none
    Word words[...];
    u8 table_end_mark; // fixed byte 0xFF
}

struct Word {
    u8 word_len;
    u8 word[word_len];
    u32 word_hash;
}

struct ReverseLookupList {
    u32 next_rlist_rel_offset; // or fixed 0xEEEEEEEE if there is none
    u32 word_hash_start;
    u32 word_hash_end;
    u32 word_offsets[word_hash_end-word_hash_start]; 
}
```

The format contains two different lookup mechanisms: a "k3c" (short for "3-character key") lookup table system for narrowing down a word hash search (getting the hash provided the word text) to all entries with the first 3 query characters, and a reverse lookup system in order to perform the reverse search. 

These two mechanisms are also combined with linked-list-like storage, allowing for the potential inclusion of new words if new documents were to be indexed without having to re-index everything again. Note that this feature didn't get to be implemented in this PoC, yet the designed formats should in principle support it nicely.

The initial 3-index-table and reverse-list offsets point to the first entries of each lookup mechanisms, with the next entries being accessible by looking at the relative offset value at the beginning of every entry.

The reverse-lookup list is split into several `ReverseLookupList` chunks, knowing that word hashes are nothing more than incremental integers. The first list chunk will contain entries from `0` to some value `H`, then the next one will contain values from `H+1` to `2*H`, and so on. Since the chunks are fixed in size, this also allows for fast reverse lookup of a given word hash, narrowing it down to the chunk of nearby indices and thus reducing the search space.
