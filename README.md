# Kintsugi

A password manager built on a custom B+tree storage engine, written from scratch in C++.

Passwords are stored in raw binary pages on disk — no SQLite, no JSON, no third-party database. The storage layer (page I/O, buffer pool, B+tree, WAL) is all hand-written.

## Building
```bash
mkdir build && cd build
cmake ..
make
```

## License
MIT
