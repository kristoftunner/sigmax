  # HFT Trading Software — Design Doc
 
**Status:** Draft
**Purpose:** Learning project
**Languages:** C++ and Rust
 
---
 
## 1. Goal & Scope
 
This is a **learning project**. The objective is to understand the architecture of a high-frequency trading system end to end — feed handling, order book reconstruction, lock-free inter-thread communication, the strategy/risk/OMS separation, and a C++/Rust FFI boundary — **not** to compete on absolute latency.
 
Crypto is the target asset class because it offers free open market data, public API specs, free testnets for real order submission, and 24/7 markets. The microstructure concepts are identical to equities/futures; only the absolute latency numbers differ (crypto is dominated by internet RTT and matching-engine jitter in the millisecond range, so kernel-bypass NICs and FPGAs are out of scope).
 
**In scope:** correct book reconstruction, lock-free MPSC queue, strategy/risk/OMS structure, FFI design, testnet order submission.
**Out of scope:** co-location, kernel bypass, nanosecond optimization, production-grade reliability.
 
---
 
## 2. Target Venue
 
- **Venue:** Binance (or Bybit) — chosen for thorough public docs, generous rate limits, free testnet, high liquidity, and abundant reference code.
- **Market data:** WebSocket diff-depth stream (incremental book updates) seeded by a REST snapshot — the same snapshot-then-incremental pattern used by real ITCH-style feeds.
- **Order submission:** Exchange **testnet** (real order flow, fake money, real fills).
---
 
## 3. Architecture Overview
 
### Data flow
 
```
WebSocket feed → JSON parse → BookEvent → [MPSC queue] → Book builder → Strategy
                                                                            │
        Order Gateway → testnet  ←  OMS  ←  Risk checks  ←  [queue]  ←──────┘
```
 
### Components
 
1. **Feed handler (I/O thread, one per WS connection)** — receives WebSocket frames, parses JSON, builds flat `BookEvent` messages, pushes to the MPSC queue. I/O and parsing only; owns no book state.
2. MPSC queues - atomic, lock-free queues, storgin POD order books, C++ implementation
3. **Order book builder (single consumer thread)** — pops events from the MPSC queue, runs the sequence/resync state machine, applies updates to the book, publishes top-of-book to the strategy. Owns all book state, so no locking on the book.
4. **Strategy engine** — consumes book state, emits order intents. Allocation-free in steady state; preallocate at startup. Must refuse to act unless the book state is `Live`.
5. **Risk / pre-trade checks** — inline, synchronous: max position, max order size, fat-finger price bands, order-rate throttle, kill switch. Never allocates or locks.
6. **OMS / Order gateway** — session management, sequence numbers, order state machine, serialization to the venue.
7. **Control plane (off hot path)** — telemetry/logging (via a separate SPSC channel to a logging thread, never blocking the trading threads), config, PnL/position reconciliation, monitoring.
---
 
## 4. Language Split (C++ / Rust)
 
The two-language split is itself part of the learning goal. Working division:
 
- **C++:** hottest I/O path and anything leaning on mature C/C++ vendor libraries (WebSocket client, JSON parsing, gateway serialization).
- **Rust:** strategy logic, risk engine, order book logic, control plane — anywhere memory-safety bugs are expensive and the performance ceiling is the same.
**Principle:** keep the FFI surface small and boring. A handful of wide, POD-only, value-passing calls is far easier to debug than many small pointer-passing ones. Pick one owner per struct; never hand-edit both sides.
 
---
 
## 5. Inter-thread Communication
 
- **MPSC queue** (already implemented, lock-free): multiple feed producers → single book-builder consumer. Route by `instrument_id`.
- Messages crossing the queue are **flat POD** (`#[repr(C)]`), no heap, fixed-size.
- **SPSC** lock-free channel for logging off the hot path.
### Message type
 
```rust
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PriceLevel {
    pub price: i64,   // scaled fixed-point integer, never float
    pub qty: i64,     // scaled fixed-point integer
}
 
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BookEvent {
    pub instrument_id: u32,
    pub first_update_id: u64,   // U
    pub final_update_id: u64,   // u
    pub n_bids: u16,
    pub n_asks: u16,
    pub bids: [PriceLevel; 32], // fixed cap; split oversized updates
    pub asks: [PriceLevel; 32],
}
```
 
**Key decisions:**
- **Scaled integers, never floats** — convert price/qty strings to fixed-point `i64` at parse time. Floats break equality/comparison and aren't deterministic.
- **Fixed-size arrays** — keeps the struct POD and queue/FFI-friendly. Updates larger than the cap are split into multiple events.
---
 
## 6. Order Book Reconstruction
 
The central correctness challenge: stitch the WebSocket incremental stream and the REST snapshot together with no gaps or overlaps.
 
### Alignment rule (Binance semantics)
 
- Buffer stream events **before** fetching the snapshot.
- Drop buffered events where `u <= lastUpdateId` (already in snapshot).
- First applied event must satisfy `U <= lastUpdateId + 1 <= u`.
- Thereafter each event's `U` must equal previous event's `u + 1`. Any gap → discard book, resync.
- Per level: `qty == 0` means **delete** the level; otherwise insert/overwrite.
### Book structure (learning phase)
 
```rust
struct OrderBook {
    bids: BTreeMap<i64, i64>,   // price -> qty, descending
    asks: BTreeMap<i64, i64>,   // price -> qty, ascending
    last_update_id: u64,
    state: BookState,           // Syncing | Live | Stale
}
```
 
`BTreeMap` is correct and fine to learn with; replace with fixed top-N arrays + overflow later if optimizing.
 
### Resync state machine
 
```
start → Syncing (buffer WS events, fetch snapshot, drop stale, find first valid)
      → Live    (apply events, check U == prev_u + 1, publish top-of-book)
      → Stale   (on sequence gap: discard book) → back to Syncing
```
 
The strategy must be told the book is not tradable while `Syncing`/`Stale` — publish the state, don't just stop sending updates, or the strategy quotes on a frozen book.
 
REST snapshot fetch runs off the consumer thread so a slow HTTP call never stalls event consumption; the book thread buffers into a per-instrument staging area until the snapshot arrives, then drains through the alignment rule.
 
---
 
## 7. FFI Design
 
- **Single source of truth:** define structs in Rust with `#[repr(C)]`, generate the C++ header with `cbindgen` (wired into `build.rs` so it can't go stale).
- **Explicit padding** in structs so layout is visible, not compiler-hidden.
- **Layout assertions on both sides** — Rust `size_of`/`align_of`/`offset_of` tests, C++ `static_assert` + `offsetof` — turning silent corruption into a loud startup/compile failure.
- **Pass POD by value**, not by pointer, on the hot path — no shared lifetimes to manage.
- **Panic/exception containment:** wrap every Rust `extern "C"` entry point (`catch_unwind` or `panic=abort`); return error codes, never let a panic or C++ exception cross the boundary (that's UB).
- **Only FFI-safe types** in extern structs: fixed-size primitives, fixed arrays, other `#[repr(C)]` structs. No `String`, `Vec`, Rust `enum`, trait objects. Enable the `improper_ctypes` lint as an error.
---
 
## 8. Known Debugging Risks
 
Ordered by expected pain:
 
1. **Struct layout mismatch** — silent corruption, nothing crashes. Mitigated by cbindgen + dual-side layout assertions.
2. **Ownership/lifetime across the boundary** — use-after-free, double-free. Mitigated by passing POD by value; ownership never crosses on the hot path.
3. **Panics/exceptions crossing FFI** — UB, fires in least-tested error paths. Mitigated by containment policy.
4. **Tooling friction** — cross-language backtraces, sanitizer composition, two build systems. Mitigated by setting up a unified dual-language debug build *before* it's needed.
5. **Bugs that look like FFI but aren't** — most often the lock-free MPSC queue's memory ordering (`Relaxed` vs `Acquire`/`Release`) or the book sequencing logic. Same symptom as layout mismatch: occasional wrong data, only under load.
### Correctness oracle
 
Continuously compare reconstructed top-of-book against the venue's book-ticker stream (or periodic REST snapshot). They must match. This localizes *where* data first goes wrong instead of guessing which side is lying — build it early.
 
---
 
## 9. Build Order
 
1. WebSocket client → log raw frames; confirm message shapes.
2. JSON → `BookEvent` parser with scaled-integer conversion; unit-test parsing.
3. MPSC producer (feed) → consumer (book thread) wired up, counting events.
4. Book apply logic + sequence checking + resync state machine.
5. Sanity strategy: print best bid/ask + spread; refuse to act unless `Live`.
6. Correctness oracle: validate top-of-book against the venue. *(Build early.)*
7. Risk layer + OMS → submit to testnet.
8. Optimize: binary protocols (SBE), multiple venues, latency measurement.
---
 
## 10. Threading & Memory Model
 
- One I/O thread per WebSocket connection (shard instruments across sockets as needed); each is an MPSC producer.
- Single book-builder consumer thread owning all book state.
- REST snapshot fetch on a separate thread/task.
- Logging on its own thread via SPSC; trading threads never block on logging.
- Preallocate everything used on the hot path at startup; allocation-free in steady state.

