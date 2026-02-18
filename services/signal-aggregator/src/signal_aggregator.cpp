#include "signal_aggregator.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace wq::aggregator {

// WeightedAverageAggregation implementation
double WeightedAverageAggregation::aggregate(const std::vector<AlphaSignal>& signals) const {
    if (signals.empty()) {
        return 0.0;
    }
    
    // Filter by confidence threshold
    std::vector<AlphaSignal> filtered;
    std::copy_if(signals.begin(), signals.end(), std::back_inserter(filtered),
        [](const AlphaSignal& sig) {
            return sig.confidence >= AggregatorConfig::MIN_CONFIDENCE_THRESHOLD;
        });
    
    if (filtered.empty()) {
        return 0.0;
    }
    
    // Calculate weighted average using lambda
    double weightedSum = std::accumulate(filtered.begin(), filtered.end(), 0.0,
        [](double sum, const AlphaSignal& sig) {
            return sum + (sig.signal * sig.confidence);
        });
    
    double totalWeight = std::accumulate(filtered.begin(), filtered.end(), 0.0,
        [](double sum, const AlphaSignal& sig) {
            return sum + sig.confidence;
        });
    
    return weightedSum / totalWeight;
}

// MedianAggregation implementation
double MedianAggregation::aggregate(const std::vector<AlphaSignal>& signals) const {
    if (signals.empty()) {
        return 0.0;
    }
    
    // Filter by confidence threshold
    std::vector<double> values;
    std::transform(signals.begin(), signals.end(), std::back_inserter(values),
        [](const AlphaSignal& sig) {
            return sig.confidence >= AggregatorConfig::MIN_CONFIDENCE_THRESHOLD ? sig.signal : 0.0;
        });
    
    // Remove zeros
    values.erase(std::remove(values.begin(), values.end(), 0.0), values.end());
    
    if (values.empty()) {
        return 0.0;
    }
    
    // Sort and find median using lambda comparator
    std::sort(values.begin(), values.end(), [](double a, double b) { return a < b; });
    
    size_t mid = values.size() / 2;
    if (values.size() % 2 == 0) {
        return (values[mid - 1] + values[mid]) / 2.0;
    } else {
        return values[mid];
    }
}

// SignalAggregator implementation
SignalAggregator::SignalAggregator(std::unique_ptr<IAggregationStrategy> strategy)
    : strategy_(std::move(strategy)) {}

void SignalAggregator::addSignal(AlphaSignal&& signal) {
    std::lock_guard<std::mutex> lock(signalsMutex_);
    
    auto& signals = signalsBySymbol_[signal.symbol];
    signals.push_back(std::move(signal));
    
    // Limit number of signals per symbol
    if (signals.size() > static_cast<size_t>(AggregatorConfig::MAX_SIGNALS_PER_SYMBOL)) {
        signals.erase(signals.begin());
    }
}

std::vector<TargetPosition> SignalAggregator::generateTargetPortfolio() {
    std::lock_guard<std::mutex> lock(signalsMutex_);
    
    std::vector<TargetPosition> portfolio;
    portfolio.reserve(signalsBySymbol_.size());
    
    // Use lambda with std::transform
    std::transform(signalsBySymbol_.begin(), signalsBySymbol_.end(),
        std::back_inserter(portfolio),
        [this](const auto& pair) {
            TargetPosition pos;
            pos.symbol = pair.first;
            pos.targetQuantity = strategy_->aggregate(pair.second) * 1000.0;  // Scale signal
            pos.currentQuantity = 0.0;
            pos.timestampNs = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            return pos;
        });
    
    return portfolio;
}

std::optional<double> SignalAggregator::getAggregatedSignal(std::string_view symbol) const {
    std::lock_guard<std::mutex> lock(signalsMutex_);
    
    auto it = signalsBySymbol_.find(std::string(symbol));
    if (it == signalsBySymbol_.end() || it->second.empty()) {
        return std::nullopt;
    }
    
    return strategy_->aggregate(it->second);
}

void SignalAggregator::clearSignalsOlderThan(int64_t timestampNs) {
    std::lock_guard<std::mutex> lock(signalsMutex_);
    
    // Use lambda to remove old signals
    for (auto& pair : signalsBySymbol_) {
        auto& signals = pair.second;
        signals.erase(
            std::remove_if(signals.begin(), signals.end(),
                [timestampNs](const AlphaSignal& sig) {
                    return sig.timestampNs < timestampNs;
                }),
            signals.end()
        );
    }
}

} // namespace wq::aggregator
