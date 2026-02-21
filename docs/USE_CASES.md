# Detailed Use Case Descriptions — WQ-Simulator

## Introduction

This document describes every use case in the WQ-Simulator system in concrete, step-by-step detail. It is written for someone with no prior finance or trading background.

### Key Concepts (Plain-Language Glossary)

| Term | Plain-Language Definition |
|------|--------------------------|
| **Symbol** | A short code (e.g. `AAPL`, `MSFT`) that identifies a single tradeable asset on a stock exchange |
| **Bid price** | The highest price a buyer is currently willing to pay |
| **Ask price** | The lowest price a seller is currently willing to accept |
| **Last price** | The price at which the most recent transaction occurred |
| **Volume** | The total number of shares traded during a session |
| **Alpha** | A mathematical formula that, given recent market data, produces a number between -1 and +1 suggesting whether to buy or sell a symbol |
| **Signal** | The numerical recommendation from a single alpha formula (negative = sell, positive = buy, magnitude = conviction) |
| **Confidence** | A 0–1 score attached to a signal indicating how reliable the formula judges itself to be |
| **Portfolio** | A collection of positions — how many shares of each symbol you own |
| **Target portfolio** | The desired portfolio after executing all pending trades |
| **Delta** | The difference between target quantity and current quantity; the amount that must be bought or sold |
| **NAV (Net Asset Value)** | The total dollar value of the portfolio |
| **ADV (Average Daily Volume)** | Average number of shares traded per day for a given symbol; used as a reference for "normal" order size |
| **Drawdown** | Percentage loss from the peak value of the portfolio |
| **Concentration** | The fraction of portfolio value that sits in a single asset |
| **TWAP** | Time-Weighted Average Price — a strategy that breaks a large order into equal pieces spread evenly over time |
| **Iceberg** | An order that hides its total size, showing only a small "tip" to the market at a time |
| **FIX protocol** | An industry-standard message format used to communicate orders between trading systems (simulated here) |
| **gRPC / Protobuf** | A framework for services to call each other's functions over a network in a fast, type-safe way |
| **UDP multicast** | A network technique that lets one sender deliver data to many receivers simultaneously |

---

## System Map

```
 ┌──────────────────────┐
 │  Market Data Source  │  (Python generator or real exchange feed)
 └──────────┬───────────┘
            │ UDP multicast (binary packets)
            ▼
 ┌──────────────────────────────────┐
 │  Service 1: Data Feed Handler    │  C++  :50051
 └──────────┬───────────────────────┘
            │ gRPC stream of MarketTick
            ▼
 ┌──────────────────────────────────┐
 │  Service 2: Alpha Engine Pool    │  C++  :50052
 └──────────┬───────────────────────┘
            │ gRPC AlphaSignal messages
            ▼
 ┌──────────────────────────────────┐
 │  Service 3: Signal Aggregator    │  C++  :50053
 └──────────┬───────────────────────┘
            │ gRPC TargetPortfolio
            ▼
 ┌──────────────────────────────────┐        ┌───────────────────────┐
 │  Service 4: Risk Guardian        │◄───────│  Service 5: EMS (Java)│
 │                                  │        │               :50055  │
 └──────────────────────────────────┘        └──────────┬────────────┘
            │ Approved / Rejected                       │
            └──────────────────────────────────────────►│
                                                        ▼
                                              ┌──────────────────┐
                                              │   PostgreSQL DB  │
                                              └──────────────────┘
```

---

## UC-01 — Ingest Raw Market Data

### Purpose
The Data Feed Handler is the system's front door for external market information. It listens on a UDP multicast socket and receives raw binary packets broadcast by stock exchanges (or the Python simulator). Its job is to pick up every packet and hand it to the right normalizer.

### Actors
- **Market data source** (external): Python generator script or real exchange feed
- **Data Feed Handler** (C++ service): the receiver

### Preconditions
- The Data Feed Handler has been started and has successfully joined the multicast group `239.255.0.1` on port `12345`.
- At least one `DataNormalizer` has been registered (see UC-02).

### Trigger
A UDP packet arrives on the multicast socket.

### Step-by-Step Execution Flow

1. The listener thread (started by `DataFeedHandler::start()`) blocks on the multicast socket waiting for incoming data.
2. A UDP packet arrives. The OS delivers it to the socket buffer.
3. The listener thread reads up to `Config::MAX_PACKET_SIZE` (65,536) bytes into a local stack buffer.
4. `DataFeedHandler::processPacket(data, length)` is called with a pointer to the raw bytes and their count.
5. `processPacket` examines the first byte(s) to determine which exchange the packet came from (NYSE, NASDAQ, CME, or UNKNOWN).
6. The matching `DataNormalizer` is looked up from the internal `normalizers_` list (stored as `weak_ptr`s to avoid preventing cleanup).
7. If the normalizer is still alive (the `weak_ptr` can be locked to a `shared_ptr`), `normalizer->normalize(data, length)` is called (see UC-02).
8. If normalization succeeds and returns a populated `std::optional<MarketData>`, each registered callback in `callbacks_` is invoked with the normalized data.
9. Atomic counters `packetsReceived_` and `packetsProcessed_` are incremented.
10. The listener thread returns to step 1.

### Input Data

| Field | Type | Description |
|-------|------|-------------|
| Raw bytes | `uint8_t*` | Binary payload from the exchange |
| Length | `size_t` | Number of bytes in the payload |

### Output Data
- One or more calls to registered `DataCallback` functions, each receiving a `const MarketData&` (see UC-02 for fields).

### Internal Processing Logic
- The class is **move-only** (copy constructor and assignment are deleted). This prevents accidental duplication of a socket handle.
- The listener runs on a dedicated `std::thread` stored in a `unique_ptr`, ensuring automatic cleanup on destruction.
- Two callback registration overloads exist:
  - `registerCallback(DataCallback)` — accepts any `std::function<void(const MarketData&)>`, including lambdas.
  - `registerCallback(RawDataCallback)` — accepts a legacy raw C-style function pointer `void(*)(const uint8_t*, size_t)`.
- Statistics (`packetsReceived_`, `packetsProcessed_`) use `std::atomic<int64_t>` so they can be safely read from any thread without a mutex.
- Normalizers are held as `std::vector<std::weak_ptr<DataNormalizer>>`. If a normalizer is destroyed externally, the weak pointer expires and the slot is silently skipped.

### Error Handling

| Error Scenario | System Behaviour |
|---------------|-----------------|
| Socket fails to open | `start()` returns `false`; the service logs an error and does not spin up the listener thread |
| Packet is too small to parse | `normalize()` returns `std::nullopt`; `packetsReceived_` is incremented but `packetsProcessed_` is not |
| Normalizer `weak_ptr` expired | Slot is skipped; no crash |
| Callback throws an exception | Exception is caught internally; remaining callbacks continue to be called |

### Performance Considerations
- Target latency from packet receipt to callback invocation: **< 100 µs**.
- The listener thread is CPU-pinned in production (not shown in the simulation code) to minimise context-switch overhead.
- The multicast buffer size is 1 MB (`Config::BUFFER_SIZE`) to tolerate short bursts without dropping packets.

---

## UC-02 — Normalize Exchange-Specific Market Data

### Purpose
Every exchange sends data in its own proprietary binary format. This use case converts those raw bytes into a single common `MarketData` struct that the rest of the system understands, and validates that the resulting numbers make sense.

### Actors
- **Data Feed Handler** (caller): passes raw bytes after UC-01.
- **DataNormalizer** (abstract class): defines the interface.
- **NYSENormalizer** / **NASDAQNormalizer** (concrete classes): do the actual work.

### Preconditions
- Raw bytes have been received (UC-01 step 7).

### Trigger
`DataFeedHandler::processPacket` calls `normalizer->normalize(data, length)`.

### Step-by-Step Execution Flow

#### NYSE Path (`NYSENormalizer::normalize`)
1. Check that `length` is at least the minimum expected for a NYSE packet. If not, return `std::nullopt`.
2. Use the template helper `parseField<T>(data, offset)` to read each field from the binary payload at its known byte offset:
   - Symbol string (fixed-width, padded with spaces)
   - Bid price (8-byte IEEE 754 double)
   - Ask price (8-byte double)
   - Last price (8-byte double)
   - Bid size (8-byte int64)
   - Ask size (8-byte int64)
   - Volume (8-byte int64)
   - Timestamp in nanoseconds (8-byte int64)
3. Populate a `MarketData` struct. Set `exchange = Exchange::NYSE` and `assetType = AssetType::EQUITY`.
4. Call `validate(data)` (the virtual method). The base-class implementation checks:
   - `bidPrice > 0`
   - `askPrice > 0`
   - `askPrice >= bidPrice`  
   The NYSE override may add exchange-specific rules (e.g. tick-size compliance).
5. If validation fails, return `std::nullopt`.
6. Return the populated `MarketData` wrapped in `std::optional`.

#### NASDAQ Path (`NASDAQNormalizer::normalize`)
Identical flow, but the byte offsets and field widths differ according to the NASDAQ binary specification.

### Input Data

| Field | Type | Description |
|-------|------|-------------|
| `rawData` | `const uint8_t*` | Pointer to the beginning of the binary packet |
| `length` | `size_t` | Packet size in bytes |

### Output Data — `MarketData` struct

| Field | Type | Description |
|-------|------|-------------|
| `symbol` | `std::string` | Ticker, e.g. `"AAPL"` |
| `bidPrice` | `double` | Best buy price |
| `askPrice` | `double` | Best sell price |
| `lastPrice` | `double` | Most recent trade price |
| `bidSize` | `int64_t` | Number of shares at bid |
| `askSize` | `int64_t` | Number of shares at ask |
| `volume` | `int64_t` | Total shares traded today |
| `timestampNs` | `int64_t` | Unix epoch in nanoseconds |
| `assetType` | `AssetType` enum | EQUITY / FUTURE / OPTION / UNKNOWN |
| `exchange` | `Exchange` enum | NYSE / NASDAQ / CME / UNKNOWN |

Convenience methods on `MarketData`:
- `midPrice()` → `(bidPrice + askPrice) / 2`
- `spread()` → `askPrice - bidPrice`

### Internal Processing Logic
- `parseField<T>` is a function template that uses `std::memcpy` to safely copy bytes into any POD type, avoiding undefined behaviour from misaligned reads.
- Move semantics: the returned `MarketData` uses a move constructor so the struct is transferred from the normalizer stack frame to the caller without copying strings.
- The `DataNormalizer` base class stores its type name as a `std::string` and exposes it via `getNormalizerType()`, which is non-virtual (identical for all instances of the same subclass).

### Error Handling

| Error Scenario | Behaviour |
|---------------|-----------|
| Packet too short | Return `std::nullopt` |
| Price fields contain NaN or Inf | `validate()` returns false → return `std::nullopt` |
| Bid > Ask (inverted market) | `validate()` returns false → return `std::nullopt` |

---

## UC-03 — Execute Alpha Strategies on a Market Tick

### Purpose
An *alpha strategy* is a mathematical formula that looks at recent price movements and decides whether a given stock is likely to go up or down. The Alpha Engine Pool runs up to 1,000 such strategies in parallel every time a new market tick arrives.

### Actors
- **Data Feed Handler** (upstream): triggers this use case by delivering a `MarketData` to a registered callback.
- **Alpha Engine Pool** (C++ service): orchestrator.
- **Thread Pool** (8 worker threads by default): executor.
- **IAlphaStrategy implementations**: the individual formulas.

### Preconditions
- At least one alpha strategy has been registered with the engine.
- The thread pool is running.
- A `MarketData` tick has been received via gRPC `StreamMarketData` (or directly via callback).

### Trigger
`AlphaEnginePool::onMarketData(marketData)` is called by the data feed gRPC client.

### Step-by-Step Execution Flow

1. The engine receives a `MarketData` struct (symbol, prices, volume, timestamp).
2. For every registered `IAlphaStrategy`, the engine **submits a task** to the thread pool:
   ```
   thread_pool.submit([strategy, data]() {
       auto signal = strategy->onMarketData(data);
       if (signal) { signalQueue.push(move(*signal)); }
   });
   ```
3. Worker threads pick up tasks concurrently. Up to 8 strategies run simultaneously.
4. Each strategy internally updates its own state (price history, running mean, etc.) and optionally produces an `AlphaSignal`.
5. Signals are pushed onto a lock-free signal queue.
6. A separate aggregation thread drains the queue and forwards signals via gRPC `SubmitSignal` to the Signal Aggregator (Service 3).

### Input Data

| Field | Type | Description |
|-------|------|-------------|
| `MarketData` | struct | Normalised tick (see UC-02) |

### Output Data

| Field | Type | Description |
|-------|------|-------------|
| `AlphaSignal.alphaId` | `std::string` | Unique identifier for the strategy |
| `AlphaSignal.symbol` | `std::string` | Ticker this signal refers to |
| `AlphaSignal.signal` | `double` | -1.0 (strong sell) to +1.0 (strong buy) |
| `AlphaSignal.confidence` | `double` | 0.0 (uncertain) to 1.0 (very confident) |
| `AlphaSignal.timestampNs` | `int64_t` | Nanosecond timestamp of when the signal was computed |

### Internal Processing Logic
- Each `IAlphaStrategy` is wrapped in an `AlphaWrapper<AlphaSignal, MarketData>` template class, which handles the type-safe dispatch.
- `AlphaSignal` is **move-only** (copy constructor deleted). Signals are transferred from strategy stack frames to the queue using `std::move`, eliminating string copies.
- The plugin system allows external `.so` shared libraries to provide additional alpha strategies at runtime via `dlopen`/`dlsym`. The library must export an `AlphaPluginInterface` struct containing function pointers for `createAlpha` and `destroyAlpha`.
- `std::optional<AlphaSignal>` is the return type of `onMarketData`. If a strategy has insufficient data (e.g. its warm-up window is not yet full), it returns `std::nullopt` and produces no signal.

### Error Handling

| Error Scenario | Behaviour |
|---------------|-----------|
| Strategy returns `std::nullopt` | No signal is emitted; no error is logged (expected during warm-up) |
| Strategy throws an exception | Exception is caught in the thread-pool task; the strategy is marked inactive; other strategies continue unaffected |
| Signal queue is full | The oldest signal is discarded to make room (back-pressure protection) |
| Plugin library fails to load | `dlopen` returns null; engine logs an error and continues with remaining strategies |

### Performance Considerations
- Target: each alpha completes within **1 ms** per tick.
- The thread pool size (default 8) should be tuned to the number of CPU cores available.
- `AlphaSignal` uses move semantics to avoid heap allocations in the hot path.
- Strategies maintain pre-allocated `std::vector` buffers for their price histories to avoid per-tick `new`/`delete`.

---

## UC-04 — Mean Reversion Alpha Signal Generation

### Purpose
The mean reversion alpha is based on the idea that if a stock's price has moved far from its recent average, it will probably "revert" back. When the price is unusually high, this strategy suggests selling; when unusually low, it suggests buying.

### Actors
- **Alpha Engine Pool** (caller, see UC-03)
- **MeanReversionAlpha** (strategy implementation)

### Preconditions
- `initialize()` has been called (sets `initialized_ = true`, pre-allocates price history vector).
- At least `windowSize` (default 20) data points have been received for the symbol.

### Trigger
`MeanReversionAlpha::onMarketData(data)` called by the thread pool (UC-03 step 2).

### Step-by-Step Execution Flow

1. Extract `lastPrice` from the incoming `MarketData`.
2. Append `lastPrice` to the `priceHistory_` circular buffer. If the buffer exceeds `windowSize`, remove the oldest entry.
3. If `priceHistory_.size() < windowSize`, there is not enough data yet — return `std::nullopt` (warm-up phase).
4. Compute the **mean** of all prices in the window:
   ```
   mean = sum(priceHistory_) / windowSize
   ```
5. Compute the **standard deviation**:
   ```
   variance = sum((p - mean)^2 for p in priceHistory_) / windowSize
   stdDev = sqrt(variance)
   ```
6. Compute the **z-score**: how many standard deviations the current price is from the mean:
   ```
   z = (lastPrice - mean) / stdDev
   ```
7. Map z-score to a signal in [-1, +1]:
   - `z > +2` → signal ≈ -1.0 (price is very high, expect fall → sell)
   - `z < -2` → signal ≈ +1.0 (price is very low, expect rise → buy)
   - `-2 ≤ z ≤ +2` → signal proportional to -z/2 (linearly scaled)
8. Set confidence proportional to `|z| / 3`, clamped to [0, 1].
9. Construct and return an `AlphaSignal` with the computed values.

### Input Data
`MarketData` struct (specifically `lastPrice`, `symbol`, `timestampNs`)

### Output Data
`std::optional<AlphaSignal>` — `std::nullopt` during warm-up; a fully populated signal afterwards.

### Internal Processing Logic
- `priceHistory_` is a `std::vector<double>` maintained as a sliding window.
- `calculateMean()` and `calculateStdDev()` are private helper methods — separated from `onMarketData` for testability.
- `isActive()` override returns `true` only after `initialize()` has been called.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| Standard deviation is zero (all prices identical) | Signal is set to 0.0; confidence is 0.0 |
| Window not yet full | Return `std::nullopt` |

---

## UC-05 — Momentum Alpha Signal Generation

### Purpose
The momentum alpha is based on the idea that recent trends continue. If a stock has been rising, it is likely to keep rising; if falling, it is likely to keep falling. This is the opposite intuition to mean reversion.

### Actors
- **Alpha Engine Pool** (caller)
- **MomentumAlpha** (strategy implementation)

### Preconditions
- `initialize()` has been called.
- At least 2 data points have been received (to compute a return).

### Trigger
`MomentumAlpha::onMarketData(data)` called from thread pool.

### Step-by-Step Execution Flow

1. If `lastPrice_` is `std::nullopt` (first data point), store `lastPrice_ = lastPrice` and return `std::nullopt`.
2. Compute the **return** (percentage change):
   ```
   ret = (lastPrice - lastPrice_.value()) / lastPrice_.value()
   ```
3. Append `ret` to `returns_` sliding buffer (max `lookbackPeriod` entries, default 10).
4. Update `lastPrice_` to the new price.
5. If fewer than `lookbackPeriod` returns have been accumulated, return `std::nullopt`.
6. Sum all returns in the buffer to get the **cumulative momentum**:
   ```
   momentum = sum(returns_)
   ```
7. Clamp momentum to [-1, +1] to produce the signal.
8. Set confidence = `min(|momentum| * 5, 1.0)` (stronger trends = higher confidence).
9. Return an `AlphaSignal`.

### Input / Output
Same structure as UC-04.

### Internal Processing Logic
- `lastPrice_` uses `std::optional<double>` to represent the "no previous price" state.
- All arithmetic uses `double`; no integer truncation.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| `lastPrice` is 0 (divide-by-zero risk) | Return `std::nullopt` |
| Buffer not yet full | Return `std::nullopt` |

---

## UC-06 — Aggregate Alpha Signals

### Purpose
With up to 1,000 alpha strategies producing signals for the same symbol, we need to combine all of them into a single authoritative number for each symbol. This use case describes how the Signal Aggregator does that.

### Actors
- **Alpha Engine Pool** (sender): calls `SubmitSignal` gRPC method.
- **Signal Aggregator** (C++ service): receiver and processor.
- **IAggregationStrategy** (abstract): the combination algorithm.
- **WeightedAverageAggregation** / **MedianAggregation** (concrete): the actual logic.

### Preconditions
- The Signal Aggregator service is running and listening on its gRPC port.
- An aggregation strategy has been selected and injected at construction time.

### Trigger
`SignalAggregator::addSignal(AlphaSignal&&)` is called (either directly or by the gRPC server handler).

### Step-by-Step Execution Flow

1. The incoming `AlphaSignal` is moved (not copied) into an internal `std::mutex`-protected map:
   ```
   signalsBySymbol_[signal.symbol].push_back(move(signal))
   ```
2. Old signals are periodically purged: any signal older than `AggregatorConfig::SIGNAL_EXPIRY_NS` (60 seconds) is removed.
3. Signals with confidence below `AggregatorConfig::MIN_CONFIDENCE_THRESHOLD` (0.3) are excluded from aggregation.
4. When `generateTargetPortfolio()` is called (see UC-07), for each symbol the aggregation strategy's `aggregate()` method is invoked with the filtered signal list:

   **WeightedAverageAggregation:**
   ```
   aggregated = sum(signal_i * confidence_i) / sum(confidence_i)
   ```
   Each signal is weighted by its confidence score. High-confidence signals have more influence.

   **MedianAggregation:**
   ```
   sorted_signals = sort(all signal values for this symbol)
   aggregated = middle value (median)
   ```
   More robust to extreme outliers (e.g. a single broken strategy generating a ±1 signal all the time).

### Input Data
`AlphaSignal` (alphaId, symbol, signal, confidence, timestampNs) — move-only struct.

### Output Data
`double` — aggregated signal for a symbol, in [-1, +1].

### Internal Processing Logic
- The aggregator is **move-only** to avoid accidental duplication of the mutex and strategy pointer.
- `signalsBySymbol_` is an `unordered_map<string, vector<AlphaSignal>>`. Lookup is O(1) average.
- The `IAggregationStrategy` is owned via `unique_ptr`, enabling run-time strategy swapping without changing the aggregator's other code.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| No signals exist for a symbol | `getAggregatedSignal()` returns `std::nullopt` |
| All signals below confidence threshold | Same as above |
| Mutex acquisition fails (should not happen) | Exception propagates to the gRPC handler which returns an error status |

---

## UC-07 — Generate Target Portfolio

### Purpose
After aggregating signals, the system needs to decide *how many shares* of each symbol to buy or sell. This use case converts the dimensionless signal values (-1 to +1) into concrete share quantities.

### Actors
- **Signal Aggregator** (producer)
- **Execution Management System** (consumer): subscribes to portfolio updates via `StreamTargetPortfolio`

### Preconditions
- At least one aggregated signal is available.
- Current positions are known (obtained from the EMS or initialised to zero).

### Trigger
`SignalAggregator::generateTargetPortfolio()` is called on a timer (e.g. every 100 ms) or after a configured number of new signals have arrived.

### Step-by-Step Execution Flow

1. Acquire the signals mutex to get a consistent snapshot.
2. For each symbol in `signalsBySymbol_`:
   a. Call the aggregation strategy (UC-06) to get a single aggregated signal value `s`.
   b. Compute a target dollar exposure:
      ```
      target_exposure = s * MAX_POSITION_VALUE
      ```
      (where `MAX_POSITION_VALUE` is a configured cap per symbol, e.g. $100,000).
   c. Divide by the symbol's current market price to get `targetQuantity` (number of shares).
   d. Retrieve `currentQuantity` from the position store.
3. Build a `TargetPosition` for each symbol:
   ```
   TargetPosition {
       symbol, targetQuantity, currentQuantity,
       delta = targetQuantity - currentQuantity,
       timestampNs
   }
   ```
4. Assemble all positions into a `TargetPortfolio` message.
5. Broadcast the portfolio via gRPC `StreamTargetPortfolio` to the EMS.

### Output Data — `TargetPortfolio`

| Field | Type | Description |
|-------|------|-------------|
| `positions` | `repeated TargetPosition` | One entry per symbol |
| `timestamp_ns` | `int64` | When this portfolio was generated |

Each `TargetPosition`:

| Field | Type | Description |
|-------|------|-------------|
| `symbol` | `string` | Ticker |
| `target_quantity` | `double` | Desired number of shares |
| `current_quantity` | `double` | Shares currently held |
| `delta` | `double` | Shares to buy (positive) or sell (negative) |

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| Market price for a symbol is unavailable | That symbol is excluded from the portfolio |
| Aggregated signal is `std::nullopt` | Symbol excluded |

### Performance Considerations
- Target: < 1 ms from signal snapshot to portfolio broadcast.
- The mutex is held only for the snapshot copy; aggregation computation happens outside the lock.

---

## UC-08 — Validate an Order Against Risk Rules (Risk Guardian)

### Purpose
Before any order is sent to an exchange, the Risk Guardian checks it against a set of safety rules. Think of it as an automated compliance officer that either stamps the order "approved" or "rejected" in under 50 microseconds. This prevents catastrophic mistakes like accidentally buying 1,000,000 shares instead of 1,000.

### Actors
- **EMS** (caller): calls `ValidateOrder` gRPC method with an `OrderRequest`.
- **Risk Guardian** (C++ service): validator.
- **RiskCheckAggregator** (internal): runs all enabled checks.
- **FatFingerCheck**, **DrawdownCheck**, **ConcentrationCheck** (concrete checks): individual rules.

### Preconditions
- The Risk Guardian has been built using `RiskGuardianBuilder` with at least one check enabled.
- The initial NAV (portfolio value) has been set.

### Trigger
`RiskGuardian::validateOrder(const Order&)` is called.

### Step-by-Step Execution Flow

1. Record the start time using a high-resolution clock.
2. Extract `symbol`, `quantity`, `side`, `price` from the `Order` struct.
3. Pass the order to `RiskCheckAggregator::validateAll(order)`:
   a. For each enabled `IRiskCheck`, call `check->validate(order, reason)`.
   b. If a check returns `false`, add a `ViolationType` to the result and append the reason string.
   c. The first failing check does **not** short-circuit: all checks are always run so the caller knows every rule that was broken.
4. Assemble a `RiskCheckResult`:
   - `approved = true` (all checks passed) or `false` (at least one failed)
   - `violations` = list of violation type codes
   - `reason` = concatenated human-readable explanations
5. Increment `validationCount_` (atomic, thread-safe).
6. Increment `approvedCount_` or `rejectedCount_`.
7. Return the result to the EMS.

### Input Data — `OrderRequest` (via gRPC) / `Order` (internal)

| Field | Type | Description |
|-------|------|-------------|
| `orderId` | `string` | Unique identifier |
| `symbol` | `string` | Ticker |
| `quantity` | `double` | Number of shares |
| `side` | `string` / enum | "BUY" or "SELL" |
| `price` | `double` | Limit price (0 for market orders) |
| `timestampNs` | `int64` | Order creation time |

### Output Data — `RiskCheckResult`

| Field | Type | Description |
|-------|------|-------------|
| `approved` | `bool` | `true` if all checks passed |
| `violations` | `ViolationType[]` | Which rules were violated |
| `reason` | `string` | Human-readable explanation |

### Internal Processing Logic
- `RiskCheckAggregator` is a template class parameterised on `TOrder`. The `validate` call uses `reinterpret_cast` to adapt between the template type and the concrete `Order` type.
- `RiskGuardian` is created exclusively through `RiskGuardianBuilder` (the constructor is private; `RiskGuardianBuilder` is declared a `friend`). This ensures the guardian is always fully configured before use.
- Market prices are cached in `marketPrices_` (an `unordered_map` protected by `shared_mutex`). Multiple threads can read prices simultaneously; writes are exclusive.
- The `validateBatch` template method allows an external caller to validate many orders in one call using a lambda callback, avoiding repeated gRPC overhead.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| `symbol` not found in ADV/position maps | The relevant check is skipped (conservative: the order is **not** blocked) |
| Validation exceeds 50 µs budget | A warning is logged; the result is still returned (not aborted) |
| Concurrent `updatePosition` call during validation | The `shared_mutex` ensures reads and writes do not race |

### Performance Considerations
- Target: **< 50 µs** end-to-end.
- All checks use only in-memory data structures (no disk I/O, no network calls).
- Reader-writer lock (`shared_mutex`) on prices allows many concurrent validators with minimal contention.
- Atomic counters for statistics avoid any mutex for the common stat-increment path.

---

## UC-09 — Fat Finger Check

### Purpose
A "fat finger" error is when a trader accidentally types too many zeros (e.g. orders 1,000,000 shares instead of 1,000). This check rejects any order whose size exceeds a defined fraction of the symbol's Average Daily Volume (ADV), because legitimate orders rarely need to be that large relative to normal market activity.

### Actors
- **RiskCheckAggregator** (caller, see UC-08)
- **FatFingerCheck** (this check)

### Preconditions
- ADV has been set for the symbol via `FatFingerCheck::setADV(symbol, adv)`.
- `maxAdvPercentage_` is configured (default: 5% = 0.05).

### Trigger
`FatFingerCheck::validate(order, reason)` called from UC-08 step 3a.

### Step-by-Step Execution Flow

1. Look up the ADV for `order.symbol` in `advMap_`.
2. If ADV is not found, skip this check (return `true` — order is allowed).
3. Compute the threshold:
   ```
   threshold = ADV * maxAdvPercentage_
   ```
4. If `order.quantity > threshold`:
   - Set `reason = "Order quantity X exceeds ADV threshold Y for symbol Z"`
   - Return `false` (order is rejected)
5. Otherwise return `true` (order is allowed).

### Example
- ADV for `AAPL` = 80,000,000 shares/day
- `maxAdvPercentage_` = 0.05 (5%)
- Threshold = 4,000,000 shares
- An order for 5,000,000 shares is rejected; an order for 3,000,000 shares is approved.

### Error Handling
- ADV not configured → check is skipped (safe default: allow).
- ADV = 0 (data error) → threshold is 0 → every order is rejected. This is intentionally conservative.

---

## UC-10 — Drawdown Protection Check

### Purpose
If the trading strategy has already lost more than a defined percentage of its starting capital today, further buying is blocked. The system cuts its losses rather than doubling down on a losing strategy.

### Actors
- **RiskCheckAggregator** (caller)
- **DrawdownCheck** (this check)

### Preconditions
- `updateStartOfDayNAV(nav)` has been called with today's opening portfolio value.
- P&L is updated in real-time via `updatePnL(currentPnL)`.

### Trigger
`DrawdownCheck::validate(order, reason)` called from UC-08 step 3a.

### Step-by-Step Execution Flow

1. Compute the current drawdown:
   ```
   drawdown = -currentPnL_ / startOfDayNAV_
   ```
   (A positive `drawdown` means the portfolio has lost money.)
2. If `drawdown > maxDrawdownPercentage_` (default 5%):
   - If `order.side == BUY`:
     - Set reason: `"Drawdown of X% exceeds maximum Y%. Buy orders blocked."`
     - Return `false`
   - If `order.side == SELL`: allow the order (selling to reduce exposure is fine).
3. Otherwise return `true`.

### Example
- Start of day NAV = $1,000,000
- Current P&L = -$60,000 (lost 6%)
- `maxDrawdownPercentage_` = 0.05 (5%)
- Drawdown = 6% > 5% → all new **buy** orders are blocked.

### Error Handling
- `startOfDayNAV_` is 0 (not initialised) → drawdown calculation produces Inf → check returns `false` (conservative: block). This is intentional.

---

## UC-11 — Concentration Check

### Purpose
Putting too many eggs in one basket is risky. If more than 10% of the portfolio's total value is already in a single stock, this check blocks further buying of that stock until the concentration falls below the limit.

### Actors
- **RiskCheckAggregator** (caller)
- **ConcentrationCheck** (this check)

### Preconditions
- Current position values have been set via `updatePosition(symbol, quantity, value)`.
- Total NAV has been set via `updateTotalNAV(nav)`.

### Trigger
`ConcentrationCheck::validate(order, reason)` called from UC-08 step 3a.

### Step-by-Step Execution Flow

1. Retrieve the current dollar value of the position in `order.symbol` from `positionValues_`.
2. Estimate what the position value would be *after* this order:
   ```
   new_value = positionValues_[symbol] + order.quantity * order.price
   ```
3. Compute the post-order concentration:
   ```
   concentration = new_value / totalNAV_
   ```
4. If `concentration > maxConcentrationPercentage_` (default 10%):
   - Set reason: `"Concentration would be X%, exceeding limit Y%"`
   - Return `false`
5. Otherwise return `true`.

### Example
- Total NAV = $1,000,000
- Current `MSFT` position value = $80,000 (8%)
- Incoming BUY order: 100 shares @ $150 = $15,000 additional
- New concentration = $95,000 / $1,000,000 = 9.5% < 10% → **approved**
- If the order were 200 shares @ $150 = $30,000 additional → $110,000 / $1,000,000 = 11% > 10% → **rejected**

---

## UC-12 — Execute a TWAP Order

### Purpose
*TWAP (Time-Weighted Average Price)* is used when a trader wants to buy or sell a large number of shares without moving the market price. Instead of submitting the entire order at once (which would drive the price up/down), the order is divided into equal pieces and one piece is submitted every few minutes.

### Actors
- **OrderManagementService** (EMS, Java): orchestrator.
- **TWAPAlgorithm**: order slicer.
- **ScheduledExecutorService**: timer for staggered submission.
- **DatabaseService**: persistence.
- **Risk Guardian**: risk check before each child order.

### Preconditions
- The parent order has been created with `type = TWAP`, `twapSlices > 0`, `twapDurationMinutes > 0`.
- The Risk Guardian has approved the order.

### Trigger
`OrderManagementService.submitOrder(order)` where `order.getType() == TWAP`.

### Step-by-Step Execution Flow

1. **Slice generation**: `TWAPAlgorithm.generateChildOrders(parentOrder, numSlices, durationMinutes)` is called.
   - Computes `sliceSize = quantity / numSlices`.
   - Any remainder (when quantity is not evenly divisible) is distributed one share at a time across the first `remainder` slices.
   - Creates `numSlices` child `Order` objects, each with `type = MARKET` and `status = PENDING_SUBMIT`.
   - Example: 1,000 shares / 10 slices = 100 shares each.

2. **Delay calculation**: `TWAPAlgorithm.calculateDelayBetweenSlices(durationMinutes, numSlices)`.
   - `delayMs = (durationMinutes * 60 * 1000) / numSlices`
   - Example: 10 minutes / 10 slices = 60,000 ms (1 minute between slices).

3. **Schedule submission**: For each child order at index `i`, a task is scheduled with delay `i * delayMs`:
   ```
   scheduler.schedule(() -> executeSimple(childOrder), i * delayMs, MILLISECONDS)
   ```
   - Child order 0 fires immediately (delay = 0).
   - Child order 1 fires after 1 minute, etc.

4. **Parent order status**: Set to `SUBMITTED` and persisted to PostgreSQL.

5. **Each scheduled child execution** (UC-14 + UC-15):
   - `executeSimple(childOrder)` is called.
   - After 100 ms simulated latency, the order is "filled".
   - `updateParentOrderFill(parentOrder, childOrder)` aggregates fills.
   - When all slices are filled, `parentOrder.status = FILLED`.

### Input Data

| Field | Type | Description |
|-------|------|-------------|
| `quantity` | `double` | Total shares to trade |
| `twapSlices` | `int` | Number of equal pieces |
| `twapDurationMinutes` | `int` | Total time window |
| `symbol` | `String` | Ticker |
| `side` | `Side` enum | BUY or SELL |

### Output Data
- `numSlices` child `Order` objects persisted to the database.
- Parent order updated to `SUBMITTED`, then `PARTIALLY_FILLED` as slices complete, then `FILLED`.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| Database unavailable | Child order submission is retried; parent status stays `SUBMITTED` |
| Scheduled task throws an exception | Logged; other slices continue unaffected |
| Parent order cancelled mid-execution | Remaining scheduled tasks are cancelled via `scheduler.cancel()` |

### Performance Considerations
- The `ScheduledExecutorService` uses a 10-thread pool, so up to 10 TWAP orders can be in-flight simultaneously.
- Timer accuracy is limited by OS scheduling (~10 ms jitter). For sub-second TWAP, a higher-resolution scheduler would be needed.

---

## UC-13 — Execute an Iceberg Order

### Purpose
An *iceberg order* hides the total order size from other market participants. Only a small "tip" (visible portion) is shown at a time. When one tip is fully executed, the next tip is automatically submitted. This is used to avoid telegraphing large buy or sell intentions to the market.

### Actors
- **OrderManagementService**: orchestrator.
- **IcebergAlgorithm**: chunk generator.
- **ScheduledExecutorService**: monitors fill status.
- **DatabaseService**: persistence.

### Preconditions
- Parent order has `type = ICEBERG` and `icebergVisibleSize > 0`.
- Risk Guardian has approved the order.

### Trigger
`OrderManagementService.submitOrder(order)` where `order.getType() == ICEBERG`.

### Step-by-Step Execution Flow

1. **Chunk generation**: `IcebergAlgorithm.generateChildOrders(parentOrder, visibleSize)`:
   - Initialise `remainingQty = parentOrder.quantity`.
   - While `remainingQty > 0`:
     - Create a child `Order` with `quantity = min(visibleSize, remainingQty)`, `type = LIMIT`, `price = parentOrder.price`.
     - Append to list.
     - `remainingQty -= thisOrderSize`
   - Example: 10,000 shares with `visibleSize = 500` → 20 child orders of 500 shares each.

2. **Submit first chunk**: `executeSimple(chunks[0])` is called immediately. This child order is the only one visible to the market at this point.

3. **Start monitoring**: `scheduleIcebergMonitoring(parentOrder, chunks, 0)` sets up a recurring 1-second check:
   - `IcebergAlgorithm.shouldReleaseNextChunk(currentChunk)` returns `true` when `currentChunk.isFilled()` (i.e. `filledQuantity ≈ quantity`).
   - When `true`: submit `chunks[currentIndex + 1]`, update parent fill count, and recursively call `scheduleIcebergMonitoring` for the next chunk.
   - This continues until all chunks are submitted.

4. **Parent order completion**: When the last chunk fills, `updateParentOrderFill` sets `parentOrder.status = FILLED`.

### Input Data

| Field | Type | Description |
|-------|------|-------------|
| `quantity` | `double` | Total shares |
| `icebergVisibleSize` | `double` | Size of each visible slice |
| `price` | `double` | Limit price |

### Output Data
- Multiple `LIMIT` child orders persisted sequentially to the database.
- Parent status transitions: `NEW → SUBMITTED → PARTIALLY_FILLED (×N) → FILLED`.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| A chunk is partially filled (only some shares execute) | The monitoring loop continues until the chunk is fully filled before releasing the next chunk |
| Price moves away from the limit price | Chunk stays open (unfilled) until the price returns. In a real system, an expiry/cancel mechanism would be added |
| All chunks submitted but parent not marked filled | A final reconciliation scan flags the discrepancy and alerts operations |

---

## UC-14 — Persist Order to Database

### Purpose
Every order, child order, and execution record is saved to PostgreSQL. This provides an audit trail, supports recovery after a crash, and enables reporting.

### Actors
- **OrderManagementService** (caller)
- **DatabaseService** (Java, wraps JDBC)
- **PostgreSQL** (database)

### Preconditions
- A live PostgreSQL connection pool is available.
- The schema has been initialised with the `orders`, `executions`, and `positions` tables.

### Trigger
Any of: `submitOrder`, `executeTWAP`, `executeIceberg`, `executeSimple`, `simulateFill`, `updateParentOrderFill`.

### Step-by-Step Execution Flow

1. `DatabaseService.saveOrder(order)` constructs an `INSERT ... ON CONFLICT DO UPDATE` SQL statement (upsert):
   ```sql
   INSERT INTO orders (order_id, symbol, quantity, filled_quantity,
                       side, type, status, price, created_at, updated_at)
   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
   ON CONFLICT (order_id) DO UPDATE SET
       filled_quantity = ?, status = ?, updated_at = ?
   ```
2. Parameters are bound from the `Order` object fields.
3. The statement is executed via a JDBC `PreparedStatement`.
4. On success, the connection is returned to the pool.

5. When an order fills, `DatabaseService.recordExecution(orderId, symbol, qty, price)` inserts into `executions`:
   ```sql
   INSERT INTO executions (execution_id, order_id, symbol, quantity, price, executed_at)
   VALUES (gen_random_uuid(), ?, ?, ?, ?, NOW())
   ```

6. `DatabaseService.updatePosition(symbol, quantityChange, price)` upserts into `positions`:
   ```sql
   INSERT INTO positions (symbol, quantity, avg_cost, last_updated)
   VALUES (?, ?, ?, NOW())
   ON CONFLICT (symbol) DO UPDATE SET
       quantity = positions.quantity + ?,
       avg_cost = (positions.avg_cost * positions.quantity + ? * ?) / (positions.quantity + ?),
       last_updated = NOW()
   ```
   (This computes the running weighted average cost.)

### Database Schema

**`orders` table**
| Column | Type | Description |
|--------|------|-------------|
| `order_id` | UUID (PK) | Unique order identifier |
| `symbol` | VARCHAR | Ticker |
| `quantity` | NUMERIC | Total shares ordered |
| `filled_quantity` | NUMERIC | Shares executed so far |
| `side` | VARCHAR | 'BUY' or 'SELL' |
| `type` | VARCHAR | 'MARKET', 'LIMIT', 'TWAP', 'ICEBERG' |
| `status` | VARCHAR | 'NEW', 'SUBMITTED', 'PARTIALLY_FILLED', 'FILLED', 'CANCELLED', 'REJECTED' |
| `price` | NUMERIC | Limit price (0 for market) |
| `created_at` | TIMESTAMP | Order creation time |
| `updated_at` | TIMESTAMP | Last modification time |

**`executions` table**
| Column | Type | Description |
|--------|------|-------------|
| `execution_id` | UUID (PK) | Unique execution identifier |
| `order_id` | UUID (FK → orders) | Parent order |
| `symbol` | VARCHAR | Ticker |
| `quantity` | NUMERIC | Shares in this execution |
| `price` | NUMERIC | Price at which shares traded |
| `executed_at` | TIMESTAMP | Execution timestamp |

**`positions` table**
| Column | Type | Description |
|--------|------|-------------|
| `symbol` | VARCHAR (PK) | Ticker |
| `quantity` | NUMERIC | Current number of shares held |
| `avg_cost` | NUMERIC | Average purchase price per share |
| `last_updated` | TIMESTAMP | Most recent update time |

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| Connection pool exhausted | `getConnection()` blocks up to the configured timeout, then throws `SQLException` |
| `SQLException` during save | Exception is caught in the service layer; error is logged; order status is set to `REJECTED` |
| Database server unreachable | Connection pool re-tries with exponential backoff; order remains in `PENDING_SUBMIT` state |

---

## UC-15 — Simulate Order Fill and Update Position

### Purpose
In a real exchange, "fill" notifications arrive as FIX execution reports from the exchange. In this simulation, fills are generated artificially after a short delay to mimic execution. After a fill, the portfolio's recorded positions are updated.

### Actors
- **OrderManagementService** (orchestrator)
- **ScheduledExecutorService** (timer)
- **DatabaseService** (persistence)

### Preconditions
- An order has been submitted (`status = SUBMITTED`).

### Trigger
`scheduler.schedule(() -> simulateFill(order), 100, MILLISECONDS)` fires 100 ms after `executeSimple` is called.

### Step-by-Step Execution Flow

1. Determine fill price:
   - If `order.price > 0` (limit order), use `order.price`.
   - Otherwise (market order), use a default of `100.0` (simplified; a real system would use the last market price from the data feed).
2. Set `fillQty = order.getRemainingQuantity()` (the entire remaining quantity is filled in one shot).
3. Update the order object:
   - `filledQuantity += fillQty`
   - `status = FILLED`
4. Persist the updated order to the database (UC-14 step 1–4).
5. Record the execution in the `executions` table (UC-14 step 5).
6. Update the position:
   - If `side == BUY`: `positionChange = +fillQty`
   - If `side == SELL`: `positionChange = -fillQty`
   - Call `databaseService.updatePosition(symbol, positionChange, fillPrice)` (UC-14 step 6).
7. Remove the order from `activeOrders` map.
8. Log: `"Order filled: {orderId, symbol, qty, price}"`.

### Output Data
- `orders` row: `status = FILLED`, `filled_quantity = quantity`.
- New `executions` row with execution details.
- Updated `positions` row with new quantity and weighted average cost.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| Database exception during fill recording | Exception is caught and logged; the order may be left in an inconsistent state (partially persisted). A reconciliation job would detect this in production. |
| `activeOrders.remove()` fails (order was already removed) | `ConcurrentHashMap.remove` is idempotent; no exception. |

---

## UC-16 — Build and Configure the Risk Guardian

### Purpose
The Risk Guardian must be set up with its limits before it can validate orders. This use case describes how to construct a fully configured Risk Guardian instance using the Builder pattern.

### Actors
- **Application startup code** (caller)
- **RiskGuardianBuilder** (builder)
- **RiskGuardian** (product)

### Trigger
Application startup or configuration reload.

### Step-by-Step Execution Flow

1. Create a `RiskGuardianBuilder` instance.
2. Chain configuration calls (fluent interface):
   ```cpp
   auto guardian = RiskGuardianBuilder()
       .withInitialNAV(1000000.0)         // $1M starting capital
       .withFatFingerCheck(0.05)           // Block orders > 5% of ADV
       .withDrawdownCheck(0.05)            // Block buys when down > 5%
       .withConcentrationCheck(0.10)       // Block if single asset > 10% NAV
       .build();
   ```
3. `build()` calls the `RiskGuardian` private constructor (possible because `RiskGuardianBuilder` is a declared `friend`).
4. Inside the constructor:
   - A `RiskCheckAggregator` is created.
   - For each enabled check, a `unique_ptr<IRiskCheck>` is instantiated with the configured limits and moved into the aggregator.
5. `build()` returns a `unique_ptr<RiskGuardian>` to the caller.

### Internal Processing Logic
- The `RiskGuardian` constructor is `private`. Only `RiskGuardianBuilder::build()` can call it (enforced at compile time via `friend` declaration). This prevents partially configured guardians.
- Each risk check holds only its configuration (no shared mutable state), making them thread-safe by default.
- `withXxx()` methods return `RiskGuardianBuilder&` (reference to self), enabling the chained syntax shown above.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| `withInitialNAV(0)` | Constructor logs a warning but proceeds. The DrawdownCheck will immediately block all buys. |
| `build()` called twice | Second call returns a new, independent `RiskGuardian` instance. |

---

## UC-17 — Batch Validation of Multiple Orders

### Purpose
The EMS may need to validate a list of orders simultaneously (e.g. all the child orders of a TWAP before scheduling any of them). This use case supports that without requiring repeated individual gRPC round-trips.

### Actors
- **EMS** (caller)
- **RiskGuardian** (validator)

### Trigger
`RiskGuardian::validateBatch(orders, callback)` called with a container of orders.

### Step-by-Step Execution Flow

1. For each `order` in `orders`:
   a. Call `validateOrder(order)` (UC-08).
   b. Invoke `callback(order, result)` with the order and its `RiskCheckResult`.
2. The caller's callback decides what to do (e.g. approve all or abort if any fails).

### Input Data
- `Container` — any iterable of `Order` objects (vector, list, etc.)
- `Callback` — a lambda or function accepting `(const Order&, const RiskCheckResult&)`

### Output Data
- Callback is invoked once per order with the individual result. No aggregated result is returned.

### Internal Processing Logic
- Implemented as a template method:
  ```cpp
  template<typename Container, typename Callback>
  void validateBatch(const Container& orders, Callback&& callback) {
      for (const auto& order : orders) {
          auto result = validateOrder(order);
          callback(order, result);
      }
  }
  ```
- `Callback&&` uses universal reference (perfect forwarding) to avoid unnecessary copies of the lambda.

---

## UC-18 — Plug In a Custom Alpha Strategy at Runtime

### Purpose
New alpha strategies can be added to a running Alpha Engine Pool without recompiling or restarting the service. This is done by loading a shared library (`*.so`) at runtime.

### Actors
- **Alpha Engine Pool** (loader)
- **Custom alpha library** (provider of strategy)

### Preconditions
- A shared library implementing the `AlphaPluginInterface` has been compiled.
- The library file path is provided to the engine.

### Trigger
`AlphaEnginePool::loadPlugin(filePath)` is called.

### Step-by-Step Execution Flow

1. Call `dlopen(filePath, RTLD_LAZY)` to load the shared library into the process.
2. Call `dlsym(handle, "getPluginInterface")` to look up the plugin registration function.
3. Cast the returned pointer to `AlphaPluginInterface*(*)()` and call it.
4. Retrieve `pluginInterface->createAlpha(config)` — a function pointer that returns a newly allocated `IAlphaStrategy*`.
5. Call `createAlpha(configJson)` to instantiate the strategy.
6. Wrap the raw pointer in a `unique_ptr<IAlphaStrategy>` using the custom deleter `pluginInterface->destroyAlpha`.
7. Register the strategy with the thread pool (same path as built-in strategies).

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| Library file not found | `dlopen` returns `nullptr`; error from `dlerror()` is logged; load aborted |
| `getPluginInterface` symbol missing | `dlsym` returns `nullptr`; library is unloaded; error logged |
| `createAlpha` returns `nullptr` | Engine logs warning; plugin is not registered |
| Plugin crashes during execution | Worker thread catches the exception; strategy is deactivated; library can be unloaded via `dlclose` |

---

## UC-19 — Stream Market Data to Alpha Engine (gRPC)

### Purpose
The Data Feed Handler and Alpha Engine Pool are separate processes (potentially on different machines). This use case describes how market data flows between them over gRPC.

### Actors
- **Data Feed Handler** (gRPC server for `MarketDataService`)
- **Alpha Engine Pool** (gRPC client)

### Protocol
gRPC server-side streaming: the client opens one long-lived connection and the server pushes `MarketTick` messages continuously.

### Step-by-Step Execution Flow

1. Alpha Engine Pool calls `stub->StreamMarketData(request, &context)` where `request` contains the list of symbols to subscribe to.
2. Data Feed Handler's `StreamMarketData` RPC handler receives the request.
3. Every time a new `MarketData` struct is produced (UC-02), the handler converts it to a `MarketTick` protobuf message:
   ```
   MarketTick {
       symbol, bid_price, ask_price, last_price,
       bid_size, ask_size, volume, timestamp_ns, exchange
   }
   ```
4. The handler calls `stream->Write(tick)` to push the message to the client.
5. The Alpha Engine Pool's reader loop receives each `MarketTick`, converts it back to an internal `MarketData` struct, and dispatches it to all registered alpha strategies (UC-03).
6. The stream remains open indefinitely. If the connection drops, the gRPC library automatically reconnects.

### Error Handling

| Scenario | Behaviour |
|----------|-----------|
| Network disconnection | gRPC retries with configurable backoff |
| Alpha Engine restarts | It re-subscribes automatically on startup |
| Symbol not found in `symbols` filter | Tick is silently dropped by the server-side filter |

---

## UC-20 — End-to-End Trading Cycle

### Purpose
This use case describes the complete lifecycle of a single trading decision, from raw market data arriving at the system's edge to a completed order in the database. It ties together all other use cases.

### Actors
All services.

### Step-by-Step Flow (Summary)

```
1. [Market] UDP multicast packet arrives
       ↓  UC-01 + UC-02
2. [Data Feed Handler] Normalises to MarketData, fires callbacks
       ↓  gRPC StreamMarketData (UC-19)
3. [Alpha Engine Pool] Distributes tick to 1000 alpha strategies (UC-03)
   - MeanReversionAlpha computes signal (UC-04)
   - MomentumAlpha computes signal     (UC-05)
       ↓  gRPC SubmitSignal
4. [Signal Aggregator] Collects signals (UC-06), generates TargetPortfolio (UC-07)
       ↓  gRPC StreamTargetPortfolio
5. [EMS] Receives TargetPortfolio, computes delta for each symbol
   For each symbol with |delta| > 0.001:
     a. Creates Order based on delta size:
        |delta| > 1000 → TWAP
        |delta| > 500  → Iceberg
        otherwise      → MARKET
     b. Calls Risk Guardian ValidateOrder (UC-08)
        - FatFingerCheck (UC-09)
        - DrawdownCheck  (UC-10)
        - ConcentrationCheck (UC-11)
     c. If approved:
        - TWAP  → UC-12
        - Iceberg → UC-13
        - Market → executeSimple
     d. Persists to database (UC-14)
       ↓  100ms simulated exchange latency
6. [EMS] Fill arrives, position updated (UC-15), database updated (UC-14)
```

### Timing Budget (End-to-End)

| Stage | Latency Budget |
|-------|---------------|
| UDP receive → callback | < 100 µs |
| Alpha strategy execution (per strategy) | < 1 ms |
| Signal aggregation | < 1 ms |
| Risk validation | < 50 µs |
| Order submission to EMS | < 10 ms |
| Simulated fill | 100 ms (artificial delay) |
| **Total (excluding simulated fill)** | **< 15 ms** |
