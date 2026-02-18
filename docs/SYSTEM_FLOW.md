# System Flow Diagram

## Data Flow Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         WQ-Simulator System                         │
└─────────────────────────────────────────────────────────────────────┘

┌──────────────┐
│ Market Data  │  UDP Multicast
│   Sources    │  239.255.0.1:12345
└──────┬───────┘
       │ Raw Binary Format
       │ (NYSE, NASDAQ, CME)
       ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Service 1: Data Feed Handler (C++)                                │
│  • UDP Multicast Listener                                           │
│  • Multi-Exchange Normalization (NYSE/NASDAQ)                       │
│  • Binary Protocol Parsing                                          │
│  Features: virtual functions, abstract class, friend, templates     │
└──────┬──────────────────────────────────────────────────────────────┘
       │ Normalized MarketData
       │ via gRPC StreamMarketData
       ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Service 2: Alpha Engine Pool (C++)                                │
│  • Thread Pool (8 workers)                                          │
│  • 1000+ Alpha Strategies                                           │
│  • Plugin System (dlopen/dlclose)                                   │
│  • Concurrent Processing                                            │
│  Features: function pointers, lambda, thread pool, unique_ptr       │
│                                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐             │
│  │ Mean         │  │  Momentum    │  │   Custom     │             │
│  │ Reversion    │  │  Alpha       │  │   Plugin     │  ... x1000  │
│  └──────────────┘  └──────────────┘  └──────────────┘             │
└──────┬──────────────────────────────────────────────────────────────┘
       │ AlphaSignals (signal, confidence)
       │ via gRPC SubmitSignal
       ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Service 3: Signal Aggregator (C++)                                │
│  • Weighted Average Strategy                                        │
│  • Median Aggregation                                               │
│  • Confidence Filtering                                             │
│  • Portfolio Optimization                                           │
│  Features: strategy pattern, optional, constexpr, move semantics    │
└──────┬──────────────────────────────────────────────────────────────┘
       │ TargetPortfolio
       │ via gRPC StreamTargetPortfolio
       ▼
       ┌───────────────────────────────────────────┐
       │                                           │
       ▼                                           ▼
┌──────────────────────────┐              ┌────────────────────────────┐
│ Service 4: Risk Guardian │◄─────────────┤ Service 5: EMS (Java)      │
│         (C++)            │ ValidateOrder│ Execution Management System│
│                          │              │                            │
│ • Fat Finger Check       │              │ ┌──────────────────────┐  │
│ • Drawdown Protection    │              │ │  Order Management    │  │
│ • Concentration Limits   │              │ │  • TWAP Algorithm    │  │
│ • <50µs Validation       │  Approved/   │ │  • Iceberg Algorithm │  │
│                          │  Rejected    │ │  • Order Slicing     │  │
│ Position Tracking:       │──────────────►│ └──────────────────────┘  │
│ • shared_ptr             │              │                            │
│ • weak_ptr               │              │ ┌──────────────────────┐  │
│ • Reader-writer locks    │              │ │  FIX Protocol (Sim)  │  │
│                          │              │ │  • Order Submission  │  │
│ Features: builder        │              │ │  • Execution Reports │  │
│ pattern, template        │              │ └──────────────────────┘  │
│ methods, atomic          │              │                            │
└──────────────────────────┘              └────────┬───────────────────┘
                                                   │
                                                   ▼
                                          ┌─────────────────┐
                                          │  PostgreSQL     │
                                          │                 │
                                          │  • orders       │
                                          │  • executions   │
                                          │  • positions    │
                                          └─────────────────┘
```

## Service Interaction Patterns

### 1. Market Data Flow (Pub-Sub)
```
Market → Data Feed → [Alpha 1, Alpha 2, ..., Alpha N]
         Handler    
```

### 2. Signal Aggregation (Map-Reduce)
```
[Alpha 1 Signal]  ┐
[Alpha 2 Signal]  ├─► Aggregator → Target Portfolio
[Alpha N Signal]  ┘
```

### 3. Risk Validation (Gatekeeper)
```
Order Request → Risk Guardian → [Approved] → EMS → Exchange
                              → [Rejected] → Client
```

### 4. Order Execution (Strategy)
```
Large Order → EMS → [TWAP: Slice over time]
                  → [Iceberg: Hide size]
                  → [Market: Execute immediately]
```

## Component Communication

### gRPC Services

```
┌──────────────────────┐
│ MarketDataService    │
│  - StreamMarketData  │◄─── Data Feed Handler
└──────────────────────┘

┌──────────────────────┐
│ AlphaSignalService   │
│  - StreamSignals     │◄─── Alpha Engine Pool
│  - SubmitSignal      │
└──────────────────────┘

┌──────────────────────┐
│ PortfolioService     │
│  - GetTarget         │◄─── Signal Aggregator
│  - StreamTarget      │
└──────────────────────┘

┌──────────────────────┐
│ RiskService          │
│  - ValidateOrder     │◄─── EMS requests validation
└──────────────────────┘
```

## C++ Feature Distribution

### Data Feed Handler
- ✅ Abstract Classes (DataNormalizer)
- ✅ Virtual Functions & vtable
- ✅ Function Templates (parseField)
- ✅ Smart Pointers (unique_ptr, shared_ptr, weak_ptr)
- ✅ Friend Classes (Factory pattern)
- ✅ Enums & Structs
- ✅ Constexpr

### Alpha Engine Pool
- ✅ Function Pointers (Plugin interface)
- ✅ Dynamic Allocation (dlopen/dlclose)
- ✅ Lambda Expressions (Thread tasks)
- ✅ Thread Pool Pattern
- ✅ Template Classes
- ✅ Move Semantics
- ✅ std::optional

### Signal Aggregator
- ✅ Strategy Pattern
- ✅ STL Algorithms with Lambdas
- ✅ Move-only Types
- ✅ Virtual Functions

### Risk Guardian
- ✅ Builder Pattern with Friend
- ✅ Template Methods
- ✅ Pass by Reference & Pointer
- ✅ Type Deduction
- ✅ Reader-Writer Locks
- ✅ Atomic Operations

## Performance Characteristics

```
Component          | Latency Target | Throughput
-------------------+----------------+------------------
Data Feed Handler  | < 100µs        | 500+ symbols
Alpha Engine       | < 1ms/alpha    | 1000+ alphas
Signal Aggregator  | < 1ms          | 10+ portfolios/sec
Risk Guardian      | < 50µs         | 10,000+ orders/sec
EMS                | < 10ms         | 1,000+ orders/sec
```

## Technology Stack per Service

```
┌────────────────────────┬──────────┬────────────────┬──────────────┐
│ Service                │ Language │ Database       │ Key Tech     │
├────────────────────────┼──────────┼────────────────┼──────────────┤
│ Data Feed Handler      │ C++17    │ -              │ UDP, gRPC    │
│ Alpha Engine Pool      │ C++17    │ -              │ dlopen, gRPC │
│ Signal Aggregator      │ C++17    │ -              │ gRPC         │
│ Risk Guardian          │ C++17    │ -              │ gRPC         │
│ EMS                    │ Java 11  │ PostgreSQL     │ JDBC, gRPC   │
│ Market Data Generator  │ Python 3 │ -              │ UDP          │
└────────────────────────┴──────────┴────────────────┴──────────────┘
```

## Deployment Topology

```
┌─────────────────────────────────────────────────────────────┐
│                    Docker Compose Network                    │
│                                                              │
│  ┌─────────────┐   ┌─────────────┐   ┌──────────────┐     │
│  │ PostgreSQL  │   │ TimescaleDB │   │ Data Feed    │     │
│  │ :5432       │   │ :5433       │   │ Handler      │     │
│  └─────────────┘   └─────────────┘   │ :50051       │     │
│                                       └──────────────┘     │
│                                                              │
│  ┌─────────────┐   ┌─────────────┐   ┌──────────────┐     │
│  │ Alpha       │   │ Signal      │   │ Risk         │     │
│  │ Engine      │   │ Aggregator  │   │ Guardian     │     │
│  │ :50052      │   │ :50053      │   │ :50054       │     │
│  └─────────────┘   └─────────────┘   └──────────────┘     │
│                                                              │
│  ┌─────────────┐                                            │
│  │ EMS (Java)  │                                            │
│  │ :50055      │                                            │
│  └─────────────┘                                            │
└─────────────────────────────────────────────────────────────┘
```

## Build & Run Sequence

```
1. Build C++ Services
   ./scripts/build.sh
   ↓
2. Build Java EMS
   ./scripts/build-ems.sh
   ↓
3. Start Databases
   docker-compose up -d
   ↓
4. Start Services (in order)
   Risk Guardian → Signal Aggregator → Alpha Engine → Data Feed Handler → EMS
   ↓
5. Generate Market Data
   python3 scripts/market_data_generator.py
   ↓
6. Monitor Logs
   Watch each service's output
```
