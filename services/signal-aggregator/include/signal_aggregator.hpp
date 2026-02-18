#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <mutex>

namespace wq::aggregator {

// Signal from alpha engine
struct AlphaSignal {
    std::string alphaId;
    std::string symbol;
    double signal;
    double confidence;
    int64_t timestampNs;
};

// Target position for portfolio
struct TargetPosition {
    std::string symbol;
    double targetQuantity;
    double currentQuantity;
    int64_t timestampNs;
};

// Abstract signal aggregation strategy
class IAggregationStrategy {
public:
    virtual ~IAggregationStrategy() = default;
    
    // Pure virtual function for aggregating signals
    virtual double aggregate(const std::vector<AlphaSignal>& signals) const = 0;
    
    virtual std::string_view getStrategyName() const = 0;
};

// Weighted average aggregation
class WeightedAverageAggregation : public IAggregationStrategy {
public:
    WeightedAverageAggregation() = default;
    ~WeightedAverageAggregation() override = default;
    
    double aggregate(const std::vector<AlphaSignal>& signals) const override;
    std::string_view getStrategyName() const override { return "WeightedAverage"; }
};

// Median aggregation (robust to outliers)
class MedianAggregation : public IAggregationStrategy {
public:
    MedianAggregation() = default;
    ~MedianAggregation() override = default;
    
    double aggregate(const std::vector<AlphaSignal>& signals) const override;
    std::string_view getStrategyName() const override { return "Median"; }
};

// Signal aggregator service
class SignalAggregator {
public:
    explicit SignalAggregator(std::unique_ptr<IAggregationStrategy> strategy);
    
    // Move only
    SignalAggregator(SignalAggregator&&) noexcept = default;
    SignalAggregator& operator=(SignalAggregator&&) noexcept = default;
    
    // Deleted copy
    SignalAggregator(const SignalAggregator&) = delete;
    SignalAggregator& operator=(const SignalAggregator&) = delete;
    
    // Add signal
    void addSignal(AlphaSignal&& signal);
    
    // Generate target portfolio
    std::vector<TargetPosition> generateTargetPortfolio();
    
    // Get aggregated signal for a symbol
    std::optional<double> getAggregatedSignal(std::string_view symbol) const;
    
    // Clear old signals
    void clearSignalsOlderThan(int64_t timestampNs);

private:
    std::unique_ptr<IAggregationStrategy> strategy_;
    std::unordered_map<std::string, std::vector<AlphaSignal>> signalsBySymbol_;
    mutable std::mutex signalsMutex_;
};

// Constexpr configuration
namespace AggregatorConfig {
    constexpr double MIN_CONFIDENCE_THRESHOLD = 0.3;
    constexpr int MAX_SIGNALS_PER_SYMBOL = 1000;
    constexpr int64_t SIGNAL_EXPIRY_NS = 60000000000LL;  // 60 seconds
}

} // namespace wq::aggregator
