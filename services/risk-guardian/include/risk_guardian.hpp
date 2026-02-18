#pragma once

#include "risk_checks.hpp"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <unordered_map>

namespace wq::risk {

// Forward declaration for friend class
class RiskGuardianBuilder;

// Position manager with smart pointers
class PositionManager {
public:
    PositionManager() = default;
    
    // Get position - returns shared_ptr
    std::shared_ptr<Position> getPosition(std::string_view symbol);
    
    // Get position - returns weak_ptr (demonstrates weak_ptr usage)
    std::weak_ptr<Position> getPositionWeak(std::string_view symbol);
    
    // Update position
    void updatePosition(std::string_view symbol, double quantity, double price);
    
    // Get all positions
    std::vector<std::shared_ptr<Position>> getAllPositions() const;
    
    // Calculate total exposure
    double calculateTotalExposure() const;
    
    // Get statistics by reference
    void getStats(size_t& numPositions, double& totalExposure) const;
    
    // Get statistics by pointer with const correctness
    void getStats(size_t* numPositions, double* totalExposure) const;

private:
    mutable std::shared_mutex mutex_;  // Reader-writer lock
    std::unordered_map<std::string, std::shared_ptr<Position>> positions_;
};

// Main Risk Guardian class
class RiskGuardian {
public:
    // Deleted copy constructor and assignment
    RiskGuardian(const RiskGuardian&) = delete;
    RiskGuardian& operator=(const RiskGuardian&) = delete;
    
    // Move semantics
    RiskGuardian(RiskGuardian&&) noexcept = default;
    RiskGuardian& operator=(RiskGuardian&&) noexcept = default;
    
    ~RiskGuardian() = default;
    
    // Validate order - main entry point (< 50µs target)
    RiskCheckResult validateOrder(const Order& order);
    
    // Function overloading for different order types
    RiskCheckResult validateOrder(std::string_view symbol, double quantity, OrderSide side, double price);
    
    // Update position after execution
    void updatePosition(std::string_view symbol, double executedQty, double executedPrice);
    
    // Set market data for calculations
    void updateMarketPrice(std::string_view symbol, double price);
    
    // Get position manager (demonstrates const method returning non-const pointer via friend)
    PositionManager* getPositionManager() { return &positionManager_; }
    const PositionManager* getPositionManager() const { return &positionManager_; }
    
    // Get statistics with type deduction
    template<typename T>
    auto getValidationCount() const -> T {
        return static_cast<T>(validationCount_.load());
    }
    
    // Lambda-based batch validation
    template<typename Container, typename Callback>
    void validateBatch(const Container& orders, Callback&& callback) {
        for (const auto& order : orders) {
            auto result = validateOrder(order);
            callback(order, result);
        }
    }

private:
    friend class RiskGuardianBuilder;  // Friend class for builder pattern
    
    // Private constructor - only builder can create
    explicit RiskGuardian(double initialNAV);
    
    PositionManager positionManager_;
    RiskCheckAggregator<Order> checkAggregator_;
    
    mutable std::mutex validationMutex_;
    std::atomic<uint64_t> validationCount_{0};
    std::atomic<uint64_t> approvedCount_{0};
    std::atomic<uint64_t> rejectedCount_{0};
    
    std::unordered_map<std::string, double> marketPrices_;
    mutable std::shared_mutex pricesMutex_;
    
    double currentNAV_;
    
    // Helper to calculate order value
    double calculateOrderValue(const Order& order) const;
};

// Builder pattern for RiskGuardian construction
class RiskGuardianBuilder {
public:
    RiskGuardianBuilder() = default;
    
    // Fluent interface
    RiskGuardianBuilder& withInitialNAV(double nav) {
        initialNAV_ = nav;
        return *this;
    }
    
    RiskGuardianBuilder& withFatFingerCheck(double maxAdvPercentage = 0.05) {
        fatFingerEnabled_ = true;
        fatFingerMaxAdv_ = maxAdvPercentage;
        return *this;
    }
    
    RiskGuardianBuilder& withDrawdownCheck(double maxDrawdownPercentage = 0.05) {
        drawdownEnabled_ = true;
        drawdownMax_ = maxDrawdownPercentage;
        return *this;
    }
    
    RiskGuardianBuilder& withConcentrationCheck(double maxConcentrationPercentage = 0.10) {
        concentrationEnabled_ = true;
        concentrationMax_ = maxConcentrationPercentage;
        return *this;
    }
    
    // Build with move semantics
    std::unique_ptr<RiskGuardian> build();

private:
    double initialNAV_{1000000.0};  // Default 1M
    bool fatFingerEnabled_{false};
    double fatFingerMaxAdv_{0.05};
    bool drawdownEnabled_{false};
    double drawdownMax_{0.05};
    bool concentrationEnabled_{false};
    double concentrationMax_{0.10};
};

// Constexpr risk limits
namespace RiskLimits {
    constexpr double DEFAULT_MAX_ADV_PERCENTAGE = 0.05;     // 5%
    constexpr double DEFAULT_MAX_DRAWDOWN = 0.05;           // 5%
    constexpr double DEFAULT_MAX_CONCENTRATION = 0.10;      // 10%
    constexpr double DEFAULT_INITIAL_NAV = 1000000.0;       // 1M
    constexpr int64_t MAX_VALIDATION_TIME_NS = 50000;       // 50µs
}

} // namespace wq::risk
