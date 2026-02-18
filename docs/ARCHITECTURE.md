# Architecture Overview

## System Design

The WQ-Simulator implements a multi-service trading architecture inspired by WorldQuant's quantitative trading platform.

## Data Flow

```
Market Data (UDP) → Data Feed Handler → Alpha Engine Pool
                                              ↓
                                         Signals
                                              ↓
                                    Signal Aggregator
                                              ↓
                                      Target Portfolio
                                              ↓
                                      Risk Guardian → EMS (Java)
                                              ↓              ↓
                                          Approved      PostgreSQL
                                              ↓
                                      Exchange (Simulated)
```

## Service Details

### 1. Data Feed Handler (C++)

**Purpose**: Ingest and normalize raw market data

**Key Features**:
- UDP multicast listener
- Multi-exchange support (NYSE, NASDAQ, CME)
- Binary protocol parsing
- Low-latency broadcast (<100µs)

**C++ Features Demonstrated**:
- Abstract base class `DataNormalizer` with virtual functions
- Concrete implementations for each exchange
- Template functions for type-safe parsing
- Smart pointers (unique_ptr, shared_ptr, weak_ptr)
- Move semantics for efficient data transfer
- Friend class for factory pattern

**Architecture Patterns**:
- Factory Pattern: `DataFeedHandlerFactory`
- Strategy Pattern: Exchange-specific normalizers
- Observer Pattern: Callbacks for data subscribers

### 2. Alpha Engine Pool (C++)

**Purpose**: Execute thousands of alpha strategies concurrently

**Key Features**:
- Thread pool for parallel execution
- Plugin system for dynamic alpha loading
- Support for 1000+ strategies
- Signal generation with confidence scores

**C++ Features Demonstrated**:
- Abstract base class `IAlphaStrategy`
- Plugin interface with function pointers
- Dynamic library loading (dlopen/dlclose)
- Template classes for generic wrappers
- Lambda expressions for thread tasks
- Move-only semantics for signals
- std::optional for return values

**Architecture Patterns**:
- Strategy Pattern: Pluggable alpha algorithms
- Template Method Pattern: Base alpha lifecycle
- Thread Pool Pattern: Concurrent execution

**Scalability**:
- Uses thread pool to process 1000 alphas
- Each alpha runs independently
- Signals aggregated asynchronously

### 3. Signal Aggregator (C++)

**Purpose**: Combine noisy alpha signals into coherent portfolio

**Key Features**:
- Multiple aggregation strategies (weighted average, median)
- Confidence-based filtering
- Portfolio optimization

**C++ Features Demonstrated**:
- Abstract aggregation strategy interface
- STL algorithms with lambda expressions
- Move semantics for signal handling
- std::optional for optional signals
- Constexpr configuration

**Architecture Patterns**:
- Strategy Pattern: Pluggable aggregation algorithms
- Template Method Pattern: Common aggregation flow

**Algorithms**:
- **Weighted Average**: Confidence-weighted signal combination
- **Median**: Robust to outlier signals

### 4. Risk Guardian (C++)

**Purpose**: Pre-trade risk management and compliance

**Key Features**:
- <50µs validation latency requirement
- Multiple risk checks (Fat Finger, Drawdown, Concentration)
- Real-time position tracking
- Atomic operations for thread safety

**C++ Features Demonstrated**:
- Abstract base class for risk checks
- Builder pattern with friend class
- Smart pointers (shared_ptr for positions, weak_ptr to avoid cycles)
- Reader-writer locks (shared_mutex)
- Pass by reference and pointer
- Template methods with type deduction
- Constexpr limits

**Architecture Patterns**:
- Builder Pattern: Fluent risk guardian construction
- Chain of Responsibility: Multiple risk checks
- Observer Pattern: Position updates

**Risk Checks**:
1. **Fat Finger**: Prevents orders exceeding X% of ADV
2. **Drawdown**: Blocks buying when strategy down >5%
3. **Concentration**: Prevents single asset >10% of NAV

### 5. Execution Management System (Java)

**Purpose**: Intelligent order slicing and execution

**Key Features**:
- TWAP (Time-Weighted Average Price) algorithm
- Iceberg order slicing
- FIX protocol support (simulated)
- PostgreSQL persistence

**Architecture Patterns**:
- Strategy Pattern: TWAP vs Iceberg
- State Pattern: Order lifecycle
- Repository Pattern: Database access

**Algorithms**:

#### TWAP (Time-Weighted Average Price)
- Slices large orders into N equal parts
- Executes over specified time period
- Minimizes market impact

```
Example: 1000 shares over 10 minutes
→ 10 orders of 100 shares each, 1 minute apart
```

#### Iceberg
- Shows only small visible portion
- Releases next chunk when current fills
- Hides total order size from market

```
Example: 10,000 shares with 500 visible
→ Submit 500, when filled submit next 500, etc.
```

**Database Schema**:
```sql
orders (
    order_id, symbol, quantity, filled_quantity,
    side, type, status, price, created_at, updated_at
)

executions (
    execution_id, order_id, symbol,
    quantity, price, executed_at
)

positions (
    symbol, quantity, avg_cost, last_updated
)
```

## Communication

### gRPC Services

All services communicate via gRPC using Protocol Buffers:

1. **MarketDataService**: Stream market ticks
2. **AlphaSignalService**: Stream alpha signals
3. **PortfolioService**: Stream target portfolios
4. **RiskService**: Validate orders

### Message Flow

```
1. Market Data → Data Feed Handler
   - UDP multicast (239.255.0.1:12345)
   - Binary format specific to each exchange

2. Data Feed Handler → Alpha Engine Pool
   - gRPC: StreamMarketData
   - Proto: MarketTick

3. Alpha Engine Pool → Signal Aggregator
   - gRPC: SubmitSignal
   - Proto: AlphaSignal

4. Signal Aggregator → Risk Guardian + EMS
   - gRPC: StreamTargetPortfolio
   - Proto: TargetPortfolio

5. Risk Guardian ← EMS
   - gRPC: ValidateOrder
   - Proto: OrderRequest → RiskCheckResult

6. EMS ↔ PostgreSQL
   - JDBC
   - SQL queries for persistence
```

## Performance Characteristics

### Latency Requirements

| Component | Target | Measured |
|-----------|--------|----------|
| Data normalization | <100µs | TBD |
| Alpha signal generation | <1ms | TBD |
| Signal aggregation | <1ms | TBD |
| Risk validation | <50µs | TBD |
| Order submission | <10ms | TBD |

### Throughput

| Component | Capacity |
|-----------|----------|
| Market data ingestion | 500+ symbols |
| Alpha strategies | 1000+ concurrent |
| Risk checks | 10,000+ orders/sec |
| Order execution | 1000+ orders/sec |

## Concurrency Model

### Data Feed Handler
- Single listener thread
- Callback-based notification
- Lock-free where possible

### Alpha Engine Pool
- Thread pool (8 workers default)
- Lock-free signal queue
- Each alpha is stateful but isolated

### Signal Aggregator
- Mutex-protected signal storage
- Batch portfolio generation
- Lock minimization

### Risk Guardian
- Reader-writer locks for positions
- Atomic counters for statistics
- Lock-free validation when possible

### EMS
- ScheduledExecutorService for TWAP
- Asynchronous order monitoring
- Connection pooling for database

## Fault Tolerance

### Service Independence
- Each service can restart independently
- gRPC handles reconnection
- State persisted where needed

### Data Persistence
- EMS persists to PostgreSQL
- Market data optionally to TimescaleDB
- Risk state maintained in memory (fast restart)

### Error Handling
- Graceful degradation
- Circuit breakers for external services
- Logging at all levels

## Scalability

### Horizontal Scaling
- Multiple Data Feed Handlers (different symbols)
- Multiple Alpha Engine Pools (strategy sharding)
- Load-balanced Risk Guardians

### Vertical Scaling
- Thread pool sizing
- Memory allocation tuning
- Database connection pooling

## Technology Choices

### C++ for Hot Path
- Data Feed Handler: Sub-microsecond latency
- Alpha Engine: Parallel processing
- Risk Guardian: Ultra-low latency validation

### Java for EMS
- Rich ecosystem (QuickFIX/J)
- JDBC for PostgreSQL
- Easier algorithm implementation

### gRPC for IPC
- Type-safe with protobuf
- Bidirectional streaming
- Multi-language support

### PostgreSQL for Persistence
- ACID compliance
- Complex queries for reporting
- Battle-tested reliability
