#include "alpha_strategy.hpp"
#include <numeric>
#include <cmath>
#include <chrono>

namespace wq::alpha {

// MeanReversionAlpha implementation
MeanReversionAlpha::MeanReversionAlpha(std::string alphaId, int windowSize)
    : alphaId_(std::move(alphaId))
    , windowSize_(windowSize) {
    priceHistory_.reserve(windowSize);
}

void MeanReversionAlpha::initialize() {
    priceHistory_.clear();
    initialized_ = true;
}

void MeanReversionAlpha::shutdown() {
    priceHistory_.clear();
    initialized_ = false;
}

bool MeanReversionAlpha::isActive() const {
    return initialized_ && IAlphaStrategy::isActive();
}

std::optional<AlphaSignal> MeanReversionAlpha::onMarketData(const MarketData& data) {
    if (!initialized_) {
        return std::nullopt;
    }
    
    // Add price to history
    priceHistory_.push_back(data.price);
    if (priceHistory_.size() > static_cast<size_t>(windowSize_)) {
        priceHistory_.erase(priceHistory_.begin());
    }
    
    // Need enough data
    if (priceHistory_.size() < static_cast<size_t>(windowSize_)) {
        return std::nullopt;
    }
    
    double mean = calculateMean();
    double stdDev = calculateStdDev();
    
    if (stdDev < 1e-6) {
        return std::nullopt;  // Not enough volatility
    }
    
    // Calculate z-score
    double zScore = (data.price - mean) / stdDev;
    
    // Generate signal (mean reversion)
    double signal = -zScore;  // Negative z-score -> positive signal (buy)
    
    // Clamp signal
    signal = std::max(AlphaConfig::MIN_SIGNAL, 
                     std::min(AlphaConfig::MAX_SIGNAL, signal));
    
    // Confidence based on z-score magnitude
    double confidence = std::min(1.0, std::abs(zScore) / 3.0);
    
    // Create signal using move semantics
    AlphaSignal alphaSignal;
    alphaSignal.alphaId = alphaId_;
    alphaSignal.symbol = data.symbol;
    alphaSignal.signal = signal;
    alphaSignal.confidence = confidence;
    alphaSignal.timestampNs = data.timestampNs;
    
    lastUpdateTime_ = data.timestampNs;
    
    return alphaSignal;
}

double MeanReversionAlpha::calculateMean() const {
    return std::accumulate(priceHistory_.begin(), priceHistory_.end(), 0.0) 
           / priceHistory_.size();
}

double MeanReversionAlpha::calculateStdDev() const {
    double mean = calculateMean();
    double variance = std::accumulate(priceHistory_.begin(), priceHistory_.end(), 0.0,
        [mean](double acc, double price) {
            double diff = price - mean;
            return acc + diff * diff;
        }) / priceHistory_.size();
    
    return std::sqrt(variance);
}

// MomentumAlpha implementation
MomentumAlpha::MomentumAlpha(std::string alphaId, int lookbackPeriod)
    : alphaId_(std::move(alphaId))
    , lookbackPeriod_(lookbackPeriod) {
    returns_.reserve(lookbackPeriod);
}

void MomentumAlpha::initialize() {
    returns_.clear();
    lastPrice_.reset();
}

void MomentumAlpha::shutdown() {
    returns_.clear();
    lastPrice_.reset();
}

std::optional<AlphaSignal> MomentumAlpha::onMarketData(const MarketData& data) {
    // Calculate return if we have previous price
    if (lastPrice_.has_value()) {
        double ret = (data.price - lastPrice_.value()) / lastPrice_.value();
        returns_.push_back(ret);
        
        if (returns_.size() > static_cast<size_t>(lookbackPeriod_)) {
            returns_.erase(returns_.begin());
        }
    }
    
    lastPrice_ = data.price;
    
    // Need enough data
    if (returns_.size() < static_cast<size_t>(lookbackPeriod_)) {
        return std::nullopt;
    }
    
    // Calculate cumulative return
    double cumulativeReturn = std::accumulate(returns_.begin(), returns_.end(), 0.0);
    
    // Signal is based on momentum direction
    double signal = std::tanh(cumulativeReturn * 10.0);  // Sigmoid-like scaling
    
    // Confidence based on consistency of returns
    int positiveReturns = std::count_if(returns_.begin(), returns_.end(),
        [](double r) { return r > 0; });
    double consistency = std::abs(static_cast<double>(positiveReturns) / returns_.size() - 0.5) * 2.0;
    
    AlphaSignal alphaSignal;
    alphaSignal.alphaId = alphaId_;
    alphaSignal.symbol = data.symbol;
    alphaSignal.signal = signal;
    alphaSignal.confidence = consistency;
    alphaSignal.timestampNs = data.timestampNs;
    
    lastUpdateTime_ = data.timestampNs;
    
    return alphaSignal;
}

} // namespace wq::alpha
