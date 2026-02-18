#include "risk_guardian.hpp"
#include <algorithm>
#include <chrono>

namespace wq::risk {

// PositionManager implementation
std::shared_ptr<Position> PositionManager::getPosition(std::string_view symbol) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    std::string symbolStr(symbol);
    auto it = positions_.find(symbolStr);
    
    if (it == positions_.end()) {
        // Create new position
        auto position = std::make_shared<Position>();
        position->symbol = symbolStr;
        positions_[symbolStr] = position;
        return position;
    }
    
    return it->second;
}

std::weak_ptr<Position> PositionManager::getPositionWeak(std::string_view symbol) {
    // Return weak_ptr to avoid circular references
    return std::weak_ptr<Position>(getPosition(symbol));
}

void PositionManager::updatePosition(std::string_view symbol, double quantity, double price) {
    auto position = getPosition(symbol);
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Update average cost
    double oldQty = position->quantity;
    double newQty = oldQty + quantity;
    
    if (newQty != 0) {
        position->avgCost = ((oldQty * position->avgCost) + (quantity * price)) / newQty;
    } else {
        position->avgCost = 0;
    }
    
    position->quantity = newQty;
}

std::vector<std::shared_ptr<Position>> PositionManager::getAllPositions() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<Position>> result;
    result.reserve(positions_.size());
    
    // Use lambda with std::transform
    std::transform(positions_.begin(), positions_.end(), std::back_inserter(result),
        [](const auto& pair) {
            return pair.second;
        });
    
    return result;
}

double PositionManager::calculateTotalExposure() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    double totalExposure = 0.0;
    
    // Use lambda with std::for_each
    std::for_each(positions_.begin(), positions_.end(),
        [&totalExposure](const auto& pair) {
            const auto& pos = pair.second;
            totalExposure += std::abs(pos->quantity * pos->avgCost);
        });
    
    return totalExposure;
}

// Pass by reference
void PositionManager::getStats(size_t& numPositions, double& totalExposure) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    numPositions = positions_.size();
    totalExposure = calculateTotalExposure();
}

// Pass by pointer with const correctness
void PositionManager::getStats(size_t* numPositions, double* totalExposure) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (numPositions) {
        *numPositions = positions_.size();
    }
    if (totalExposure) {
        *totalExposure = calculateTotalExposure();
    }
}

// RiskGuardian private constructor
RiskGuardian::RiskGuardian(double initialNAV)
    : currentNAV_(initialNAV) {}

RiskCheckResult RiskGuardian::validateOrder(const Order& order) {
    // Measure validation time for 50µs requirement
    auto startTime = std::chrono::high_resolution_clock::now();
    
    validationCount_++;
    
    std::lock_guard<std::mutex> lock(validationMutex_);
    
    // Run all risk checks
    auto result = checkAggregator_.validateAll(order);
    
    if (result.approved) {
        approvedCount_++;
    } else {
        rejectedCount_++;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    
    // Log if exceeds 50µs target
    if (duration.count() > RiskLimits::MAX_VALIDATION_TIME_NS) {
        // Would log warning in production
    }
    
    return result;
}

// Function overloading
RiskCheckResult RiskGuardian::validateOrder(
    std::string_view symbol,
    double quantity,
    OrderSide side,
    double price) {
    
    Order order;
    order.symbol = symbol;
    order.quantity = quantity;
    order.side = side;
    order.price = price;
    order.timestampNs = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    
    return validateOrder(order);
}

void RiskGuardian::updatePosition(std::string_view symbol, double executedQty, double executedPrice) {
    positionManager_.updatePosition(symbol, executedQty, executedPrice);
}

void RiskGuardian::updateMarketPrice(std::string_view symbol, double price) {
    std::unique_lock<std::shared_mutex> lock(pricesMutex_);
    marketPrices_[std::string(symbol)] = price;
}

double RiskGuardian::calculateOrderValue(const Order& order) const {
    return std::abs(order.quantity * order.price);
}

// RiskGuardianBuilder implementation
std::unique_ptr<RiskGuardian> RiskGuardianBuilder::build() {
    // Use friend access to call private constructor
    auto guardian = std::unique_ptr<RiskGuardian>(new RiskGuardian(initialNAV_));
    
    // Add enabled checks using move semantics
    if (fatFingerEnabled_) {
        auto check = std::make_unique<FatFingerCheck>(fatFingerMaxAdv_);
        guardian->checkAggregator_.addCheck(std::move(check));
    }
    
    if (drawdownEnabled_) {
        auto check = std::make_unique<DrawdownCheck>(drawdownMax_);
        guardian->checkAggregator_.addCheck(std::move(check));
    }
    
    if (concentrationEnabled_) {
        auto check = std::make_unique<ConcentrationCheck>(concentrationMax_);
        guardian->checkAggregator_.addCheck(std::move(check));
    }
    
    return guardian;
}

} // namespace wq::risk
