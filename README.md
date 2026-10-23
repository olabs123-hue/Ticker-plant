# Market Data Ticker Plant

C++17 market-data ticker plant prototype with ordered feed recovery, lock-free SPSC queue, sequence-indexed append-only log and per-subscriber conflation.

## Features
- Deduplicates and reorders sequenced ticks, requests newly detected missing ranges once, and releases ticks in order. A recovery source must supply missing ticks.
- Single-producer/single-consumer lock-free ring buffer with acquire/release atomic ordering.
- Append-only binary tick log with sequence-indexed replay after reopening; truncated trailing records are discarded. **Not crash-durable per append**: no fsync, checksum, or cross-process locking.
- Subscriber-specific latest-per-symbol conflation, with no blocking wait for subscribers. Publishing cost scales with subscriber count.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/tickerplant_bench 1000000
```

GoogleTest is fetched via CMake FetchContent, requiring network access at configure time. The suite defines 31 tests. CI matrix covers GCC and Clang with no sanitizer, AddressSanitizer+UBSan, and ThreadSanitizer; check the Actions tab for actual outcomes.

## Benchmark scope
The synthetic harness uses 1M sequential ticks, a deterministic random seed, independently injected 2% loss and 2% duplication, and ~2% adjacent swaps; it supplies dropped ticks after the primary stream, checks final ordered delivery, and reports elapsed processing throughput. This is **not** live feed ingestion, exchange connectivity, production latency, end-to-end recovery timing, or a coordinated-omission-safe latency histogram. A 1M-tick benchmark result is valid only when the workflow or local command completes.

## Limitations
- No production networking or recovery request transport.
- No latency histogram or coordinated-omission correction yet.
- No fsync durability guarantee; log recovery only handles truncated trailing records.
- No multi-producer ring buffer; no subscriber threading guarantees.
