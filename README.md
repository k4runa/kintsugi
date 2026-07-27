# Kintsugi
A disk-backed B+tree storage engine, written from scratch in C++.

I wanted to actually understand how something like SQLite works
under the hood, so I'm building the storage layer of one — not a
full database, just the part that gets data on and off disk
correctly.

## Status

**Disk layer** — done. Fixed 4KB pages, read/write by page ID.

**Buffer pool** — done. LRU eviction, pin/unpin tracking, dirty
page flushing. Pages are cached in memory instead of hitting disk
on every access.

**B+tree index** — done. Search, insert, update, delete, and range
queries all work, including splitting a node when it fills up (509
keys max) and growing the tree, and merging/borrowing keys from a
sibling when a node gets too empty. It passes my tests, but I don't
fully understand the underflow logic (borrow/merge) yet — I want to
rewrite it myself from scratch before I fully trust it.

**Write-ahead log** — done, redo-only. Logs a change before it's
applied, so a crash mid-write doesn't corrupt the file. Supports
both POSIX and native Windows I/O.

**Everything else — doesn't exist yet.** No query parser, no SQL,
no transactions beyond what the WAL gives you for free, no
concurrency (single-threaded only). This is a storage engine, not
a database.

## Components

- `BTreeIndex` — the tree: insert, search, update, delete, split, range query
- `BufferPoolManager` — LRU page cache, pin/unpin, dirty tracking
- `DiskManager` — raw page I/O, 4KB pages
- `WALManager` — redo logging for crash recovery

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## License

[MIT](LICENSE)
