# WQ-Simulator

WorldQuant Trading Simulator - A comprehensive trading system simulation using microservices architecture.

## Overview

This project simulates a quantitative trading system inspired by WorldQuant's architecture, demonstrating:
- **Microservices architecture** with gRPC communication
- **Multi-language implementation** (C++, Java, Python)
- **Real-time data processing** with low-latency requirements
- **Advanced C++ features** (templates, smart pointers, move semantics, virtual functions, etc.)
- **Risk management** and compliance checks
- **Algorithmic execution** (TWAP, Iceberg)

## Architecture

### Services

1. **Data Feed Handler (C++)** - Ingests and normalizes market data via UDP multicast
2. **Alpha Engine Pool (C++)** - Runs 1000+ alpha strategies concurrently using thread pools
3. **Signal Aggregator (C++)** - Combines alpha signals into coherent portfolio targets
4. **Risk Guardian (C++)** - Pre-trade risk checks with <50µs latency requirement
5. **Execution Management System (Java)** - Slices orders using TWAP/Iceberg algorithms

### Technology Stack

- **C++17** - Core high-performance services
- **Java 11** - EMS service with PostgreSQL persistence
- **Python 3** - Utilities and data generation
- **gRPC/Protobuf** - Inter-service communication
- **PostgreSQL** - Order and execution persistence

## C++ Features Demonstrated

This project comprehensively demonstrates advanced C++ concepts:

- ✅ **Classes and Namespaces** - Organized service architecture
- ✅ **string_view** - Efficient string handling
- ✅ **Function Overloading & Templates** - Generic programming
- ✅ **Constexpr Functions** - Compile-time computation
- ✅ **References & Pointers** - Pass-by-reference/pointer patterns
- ✅ **Type Deduction** - Auto, decltype, template type deduction
- ✅ **std::optional** - Optional value handling
- ✅ **Enums & Structs** - Type-safe data structures
- ✅ **Friend Classes** - Controlled access patterns
- ✅ **Dynamic Allocation** - Manual memory management
- ✅ **Function Pointers** - Plugin architecture
- ✅ **Lambda Expressions** - Functional programming
- ✅ **Move Semantics** - Efficient resource transfer
- ✅ **Smart Pointers** - unique_ptr, shared_ptr, weak_ptr
- ✅ **Virtual Functions** - Runtime polymorphism
- ✅ **Virtual Table (vtable)** - Understanding dynamic dispatch
- ✅ **Object Slicing** - Prevention techniques
- ✅ **Virtual Destructors** - Proper cleanup in hierarchies
- ✅ **Abstract Classes** - Pure virtual interfaces
- ✅ **Template Classes** - Generic container patterns

## Project Structure

```
WQ-Simulator/
├── proto/                      # Protobuf definitions
│   ├── market_data.proto
│   ├── alpha_signals.proto
│   ├── portfolio.proto
│   └── risk.proto
├── services/
│   ├── data-feed-handler/     # C++ - Market data ingestion
│   ├── alpha-engine/          # C++ - Alpha strategy execution
│   ├── signal-aggregator/     # C++ - Signal combination
│   ├── risk-guardian/         # C++ - Risk management
│   └── ems-java/              # Java - Order execution
├── docs/
│   ├── USE_CASES.md           # Detailed use case descriptions (all 20 use cases)
│   ├── ARCHITECTURE.md        # Architecture overview
│   └── ...
├── CMakeLists.txt             # Root build configuration
└── README.md
```

## Documentation

- [`docs/USE_CASES.md`](docs/USE_CASES.md) — **Detailed use case descriptions** for all 20 use cases, including step-by-step flows, input/output data types, error handling, and performance notes. Written to be understandable without a finance or trading background.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Architecture overview and service interaction diagrams
- [`docs/SYSTEM_FLOW.md`](docs/SYSTEM_FLOW.md) — End-to-end data flow diagrams
- [`docs/QUICKSTART.md`](docs/QUICKSTART.md) — Build and run instructions

## License

MIT License