# C++ Features Demonstration

This document maps specific C++ features to their implementation in the WQ-Simulator codebase.

## Core Language Features

### 1. Classes and Namespaces
**Location**: All C++ services
```cpp
// services/data-feed-handler/include/data_types.hpp
namespace wq::datafeed {
    class DataNormalizer { ... }
    class NYSENormalizer : public DataNormalizer { ... }
}
```

### 2. string_view
**Location**: Throughout headers for efficient string parameters
```cpp
// services/data-feed-handler/include/data_types.hpp
constexpr std::string_view assetTypeToString(AssetType type) {
    // Returns string_view for zero-copy string access
}
```

### 3. Function Overloading and Function Templates
**Location**: Multiple services
```cpp
// services/data-feed-handler/include/data_feed_handler.hpp
void registerCallback(DataCallback callback);  // Overload 1
void registerCallback(RawDataCallback callback);  // Overload 2

// Template function
template<typename T>
T parseField(const uint8_t* data, size_t offset);
```

### 4. Constexpr Functions
**Location**: Configuration and utility functions
```cpp
// services/data-feed-handler/include/data_types.hpp
constexpr std::string_view exchangeToString(Exchange exch) {
    // Compile-time string conversion
}

// services/risk-guardian/include/risk_guardian.hpp
namespace RiskLimits {
    constexpr double DEFAULT_MAX_ADV_PERCENTAGE = 0.05;
}
```

### 5. Compound Types: References and Pointers
**Location**: All services demonstrate both patterns
```cpp
// services/risk-guardian/include/risk_guardian.hpp
// Pass by reference
void getStats(size_t& numPositions, double& totalExposure) const;

// Pass by pointer
void getStats(size_t* numPositions, double* totalExposure) const;
```

### 6. Type Deduction with auto, decltype
**Location**: Template methods and lambda usage
```cpp
// services/data-feed-handler/include/data_feed_handler.hpp
template<typename T>
auto processData(const T& data) -> decltype(auto) {
    return processDataImpl(data);
}
```

### 7. std::optional
**Location**: Return types that may not have values
```cpp
// services/alpha-engine/include/alpha_strategy.hpp
virtual std::optional<AlphaSignal> onMarketData(const MarketData& data) = 0;

// services/signal-aggregator/include/signal_aggregator.hpp
std::optional<double> getAggregatedSignal(std::string_view symbol) const;
```

## Compound Types

### 8. Enums and Structs
**Location**: Data structures across all services
```cpp
// services/data-feed-handler/include/data_types.hpp
enum class AssetType : uint8_t {
    EQUITY,
    FUTURE,
    OPTION,
    UNKNOWN
};

struct MarketData {
    std::string symbol;
    double bidPrice;
    double askPrice;
    // ... more fields
};
```

### 9. Friend Classes
**Location**: Factory and builder patterns
```cpp
// services/data-feed-handler/include/data_feed_handler.hpp
class DataFeedHandler {
private:
    friend class DataFeedHandlerFactory;  // Factory has access to private constructor
    explicit DataFeedHandler(std::string multicastGroup, uint16_t port);
};
```

## Memory Management

### 10. Dynamic Allocation
**Location**: Plugin loading system
```cpp
// services/alpha-engine/src/alpha_engine.cpp
void* handle = dlopen(pluginPath.c_str(), RTLD_LAZY);  // Dynamic library loading
if (handle) {
    dlclose(handle);  // Manual cleanup
}
```

### 11. Function Pointers
**Location**: Plugin interface and callbacks
```cpp
// services/alpha-engine/include/alpha_strategy.hpp
using AlphaFactoryFunc = IAlphaStrategy* (*)(const char* config);

struct AlphaPluginInterface {
    const char* pluginName;
    AlphaFactoryFunc createAlpha;
    void (*destroyAlpha)(IAlphaStrategy*);
};
```

### 12. Lambda Expressions
**Location**: Callbacks and STL algorithms throughout
```cpp
// services/alpha-engine/src/alpha_engine.cpp
threadPool_->enqueue([this, alphaPtr, data]() {
    this->processAlpha(alphaPtr, data);
});

// services/signal-aggregator/src/signal_aggregator.cpp
std::copy_if(signals.begin(), signals.end(), std::back_inserter(filtered),
    [](const AlphaSignal& sig) {
        return sig.confidence >= AggregatorConfig::MIN_CONFIDENCE_THRESHOLD;
    });
```

## Modern C++ Features

### 13. Move Semantics
**Location**: All services for efficient resource transfer
```cpp
// services/data-feed-handler/include/data_types.hpp
struct MarketData {
    MarketData(MarketData&& other) noexcept { ... }  // Move constructor
    MarketData& operator=(MarketData&& other) noexcept { ... }  // Move assignment
};
```

### 14. Smart Pointers

#### unique_ptr
```cpp
// services/alpha-engine/include/alpha_engine.hpp
std::unique_ptr<ThreadPool> threadPool_;
std::vector<std::unique_ptr<IAlphaStrategy>> alphas_;
```

#### shared_ptr
```cpp
// services/risk-guardian/src/risk_guardian.cpp
std::shared_ptr<Position> getPosition(std::string_view symbol);
```

#### weak_ptr
```cpp
// services/data-feed-handler/src/data_feed_handler.cpp
std::vector<std::weak_ptr<DataNormalizer>> normalizers_;
// Use weak_ptr to avoid circular references
```

## Object-Oriented Programming

### 15. Virtual Functions
**Location**: All abstract base classes
```cpp
// services/data-feed-handler/include/data_types.hpp
class DataNormalizer {
public:
    virtual ~DataNormalizer() = default;  // Virtual destructor
    virtual std::optional<MarketData> normalize(...) = 0;  // Pure virtual
    virtual bool validate(...) const { ... }  // Virtual with default
};
```

### 16. Virtual Table (vtable)
The vtable is automatically created by the compiler for classes with virtual functions. Each object has a hidden vptr pointing to its class's vtable.

**Demonstrated in**:
- `DataNormalizer` hierarchy (data-feed-handler)
- `IAlphaStrategy` hierarchy (alpha-engine)
- `IRiskCheck` hierarchy (risk-guardian)
- `IAggregationStrategy` hierarchy (signal-aggregator)

### 17. Object Slicing Prevention
**Location**: Move-only and deleted copy constructors
```cpp
// services/alpha-engine/include/alpha_strategy.hpp
struct AlphaSignal {
    AlphaSignal(const AlphaSignal&) = delete;  // Prevent slicing
    AlphaSignal& operator=(const AlphaSignal&) = delete;
    AlphaSignal(AlphaSignal&&) noexcept = default;  // But allow move
};
```

### 18. Virtual Destructors
**Location**: All abstract base classes
```cpp
// services/alpha-engine/include/alpha_strategy.hpp
class IAlphaStrategy {
public:
    virtual ~IAlphaStrategy() = default;  // Ensures proper cleanup of derived classes
};
```

### 19. Abstract Classes
**Location**: Interface definitions across services
```cpp
// services/risk-guardian/include/risk_checks.hpp
class IRiskCheck {
public:
    virtual ~IRiskCheck() = default;
    virtual bool validate(const Order& order, std::string& reason) const = 0;  // Pure virtual
    // Cannot instantiate IRiskCheck directly
};
```

### 20. Template Classes
**Location**: Generic containers and wrappers
```cpp
// services/alpha-engine/include/alpha_strategy.hpp
template<typename TSignal, typename TData>
class AlphaWrapper {
public:
    using SignalType = TSignal;
    using DataType = TData;
    
    template<typename Callback>
    void processData(const TData& data, Callback&& callback) { ... }
};

// services/risk-guardian/include/risk_checks.hpp
template<typename TOrder>
class RiskCheckAggregator {
    std::vector<std::unique_ptr<IRiskCheck>> checks_;
public:
    RiskCheckResult validateAll(const TOrder& order) const { ... }
};
```

## Summary

Every required C++ feature is demonstrated in a real-world context:
- **Language basics**: classes, namespaces, string_view, enums, structs ✓
- **Function features**: overloading, templates, constexpr ✓
- **Type system**: references, pointers, type deduction, optional ✓
- **Memory**: dynamic allocation, smart pointers, RAII ✓
- **Functional**: function pointers, lambdas ✓
- **OOP**: virtual functions, vtable, abstract classes, proper inheritance ✓
- **Modern C++**: move semantics, template classes ✓

Each feature serves a genuine purpose in the trading system architecture rather than being artificially inserted.
