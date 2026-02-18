#include "risk_checks.hpp"
#include <cmath>
#include <algorithm>

namespace wq::risk {

// FatFingerCheck implementation
FatFingerCheck::FatFingerCheck(double maxAdvPercentage)
    : maxAdvPercentage_(maxAdvPercentage) {}

bool FatFingerCheck::validate(const Order& order, std::string& reason) const {
    auto it = advMap_.find(order.symbol);
    if (it == advMap_.end()) {
        // No ADV data, cannot validate
        return true;
    }
    
    double adv = it->second;
    double maxAllowedQty = adv * maxAdvPercentage_;
    
    if (std::abs(order.quantity) > maxAllowedQty) {
        reason = "Order quantity " + std::to_string(order.quantity) +
                " exceeds " + std::to_string(maxAdvPercentage_ * 100) +
                "% of ADV (" + std::to_string(maxAllowedQty) + ")";
        return false;
    }
    
    return true;
}

void FatFingerCheck::setADV(std::string_view symbol, double adv) {
    advMap_[std::string(symbol)] = adv;
}

// DrawdownCheck implementation
DrawdownCheck::DrawdownCheck(double maxDrawdownPercentage)
    : maxDrawdownPercentage_(maxDrawdownPercentage) {}

bool DrawdownCheck::validate(const Order& order, std::string& reason) const {
    if (startOfDayNAV_ <= 0) {
        return true;  // No baseline
    }
    
    double currentDrawdown = -currentPnL_ / startOfDayNAV_;
    
    if (currentDrawdown > maxDrawdownPercentage_) {
        // Block buy orders when in drawdown
        if (order.side == OrderSide::BUY) {
            reason = "Strategy is in " + 
                    std::to_string(currentDrawdown * 100) + 
                    "% drawdown, exceeds limit of " +
                    std::to_string(maxDrawdownPercentage_ * 100) + "%";
            return false;
        }
    }
    
    return true;
}

void DrawdownCheck::updatePnL(double currentPnL) {
    currentPnL_ = currentPnL;
}

void DrawdownCheck::updateStartOfDayNAV(double nav) {
    startOfDayNAV_ = nav;
}

// ConcentrationCheck implementation
ConcentrationCheck::ConcentrationCheck(double maxConcentrationPercentage)
    : maxConcentrationPercentage_(maxConcentrationPercentage) {}

bool ConcentrationCheck::validate(const Order& order, std::string& reason) const {
    if (totalNAV_ <= 0) {
        return true;  // No NAV data
    }
    
    // Calculate position value after this order
    double currentValue = 0;
    auto it = positionValues_.find(order.symbol);
    if (it != positionValues_.end()) {
        currentValue = it->second;
    }
    
    double newValue = currentValue + (order.quantity * order.price);
    double concentration = std::abs(newValue) / totalNAV_;
    
    if (concentration > maxConcentrationPercentage_) {
        reason = "Order would result in " +
                std::to_string(concentration * 100) +
                "% concentration in " + order.symbol +
                ", exceeds limit of " +
                std::to_string(maxConcentrationPercentage_ * 100) + "%";
        return false;
    }
    
    return true;
}

void ConcentrationCheck::updatePosition(std::string_view symbol, double quantity, double value) {
    positionValues_[std::string(symbol)] = value;
}

void ConcentrationCheck::updateTotalNAV(double nav) {
    totalNAV_ = nav;
}

} // namespace wq::risk
