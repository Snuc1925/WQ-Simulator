#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <cstdint>

namespace wq::alpha {

// Forward declaration
struct MarketData;

// Signal structure
struct AlphaSignal {
    std::string alphaId;
    std::string symbol;
    double signal;       // -1.0 to +1.0
    double confidence;   // 0.0 to 1.0
    int64_t timestampNs;
    
    // Move semantics
    AlphaSignal() = default;
    AlphaSignal(AlphaSignal&&) noexcept = default;
    AlphaSignal& operator=(AlphaSignal&&) noexcept = default;
    
    // Deleted copy
    AlphaSignal(const AlphaSignal&) = delete;
    AlphaSignal& operator=(const AlphaSignal&) = delete;
};

// Abstract base class for Alpha strategies (demonstrates pure virtual functions)
class IAlphaStrategy {
public:
    virtual ~IAlphaStrategy() = default;  // Virtual destructor
    
    // Pure virtual functions - this is an abstract class
    virtual std::string_view getAlphaId() const = 0;
    virtual std::optional<AlphaSignal> onMarketData(const MarketData& data) = 0;
    virtual void initialize() = 0;
    virtual void shutdown() = 0;
    
    // Virtual function with default implementation
    virtual bool isActive() const {
        return true;
    }
    
    // Non-virtual public interface
    int64_t getLastUpdateTime() const {
        return lastUpdateTime_;
    }

protected:
    int64_t lastUpdateTime_{0};
};

// Simple mean reversion alpha
class MeanReversionAlpha : public IAlphaStrategy {
public:
    explicit MeanReversionAlpha(std::string alphaId, int windowSize = 20);
    ~MeanReversionAlpha() override = default;
    
    std::string_view getAlphaId() const override {
        return alphaId_;
    }
    
    std::optional<AlphaSignal> onMarketData(const MarketData& data) override;
    void initialize() override;
    void shutdown() override;
    bool isActive() const override;

private:
    std::string alphaId_;
    int windowSize_;
    std::vector<double> priceHistory_;
    bool initialized_{false};
    
    double calculateMean() const;
    double calculateStdDev() const;
};

// Momentum alpha
class MomentumAlpha : public IAlphaStrategy {
public:
    explicit MomentumAlpha(std::string alphaId, int lookbackPeriod = 10);
    ~MomentumAlpha() override = default;
    
    std::string_view getAlphaId() const override {
        return alphaId_;
    }
    
    std::optional<AlphaSignal> onMarketData(const MarketData& data) override;
    void initialize() override;
    void shutdown() override;

private:
    std::string alphaId_;
    int lookbackPeriod_;
    std::vector<double> returns_;
    std::optional<double> lastPrice_;
};

// Template class for generic alpha wrapper
template<typename TSignal, typename TData>
class AlphaWrapper {
public:
    using SignalType = TSignal;
    using DataType = TData;
    
    explicit AlphaWrapper(std::unique_ptr<IAlphaStrategy> strategy)
        : strategy_(std::move(strategy)) {}
    
    template<typename Callback>
    void processData(const TData& data, Callback&& callback) {
        if (auto signal = strategy_->onMarketData(reinterpret_cast<const MarketData&>(data))) {
            callback(std::move(signal.value()));
        }
    }
    
    const IAlphaStrategy* getStrategy() const {
        return strategy_.get();
    }

private:
    std::unique_ptr<IAlphaStrategy> strategy_;
};

// Function pointer type for alpha factory
using AlphaFactoryFunc = IAlphaStrategy* (*)(const char* config);

// Plugin interface structure
struct AlphaPluginInterface {
    const char* pluginName;
    const char* version;
    AlphaFactoryFunc createAlpha;
    void (*destroyAlpha)(IAlphaStrategy*);
};

// Constexpr configuration
namespace AlphaConfig {
    constexpr int DEFAULT_WINDOW_SIZE = 20;
    constexpr int DEFAULT_LOOKBACK = 10;
    constexpr double MIN_SIGNAL = -1.0;
    constexpr double MAX_SIGNAL = 1.0;
    constexpr double MIN_CONFIDENCE = 0.0;
    constexpr double MAX_CONFIDENCE = 1.0;
}

} // namespace wq::alpha
