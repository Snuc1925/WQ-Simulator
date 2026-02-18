#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace wq::risk {

// Order side enum
enum class OrderSide : uint8_t {
    BUY,
    SELL
};

// Risk violation types
enum class ViolationType : uint8_t {
    FAT_FINGER,
    DRAWDOWN,
    CONCENTRATION,
    POSITION_LIMIT,
    NONE
};

constexpr std::string_view violationTypeToString(ViolationType type) {
    switch (type) {
        case ViolationType::FAT_FINGER: return "FAT_FINGER";
        case ViolationType::DRAWDOWN: return "DRAWDOWN";
        case ViolationType::CONCENTRATION: return "CONCENTRATION";
        case ViolationType::POSITION_LIMIT: return "POSITION_LIMIT";
        default: return "NONE";
    }
}

// Order structure
struct Order {
    std::string orderId;
    std::string symbol;
    double quantity;
    OrderSide side;
    double price;
    int64_t timestampNs;
    
    Order() = default;
    Order(Order&&) noexcept = default;
    Order& operator=(Order&&) noexcept = default;
};

// Position tracking
struct Position {
    std::string symbol;
    double quantity;
    double avgCost;
    double unrealizedPnL;
    double realizedPnL;
    
    Position() : quantity(0), avgCost(0), unrealizedPnL(0), realizedPnL(0) {}
};

// Abstract base class for risk checks (demonstrates pure virtual functions)
class IRiskCheck {
public:
    virtual ~IRiskCheck() = default;  // Virtual destructor
    
    // Pure virtual function
    virtual bool validate(const Order& order, std::string& reason) const = 0;
    
    // Virtual function with default implementation
    virtual std::string_view getCheckName() const = 0;
    
    // Virtual function for enabling/disabling
    virtual bool isEnabled() const {
        return enabled_;
    }
    
    virtual void setEnabled(bool enabled) {
        enabled_ = enabled;
    }

protected:
    bool enabled_{true};
};

// Fat Finger check - prevents abnormally large orders
class FatFingerCheck : public IRiskCheck {
public:
    explicit FatFingerCheck(double maxAdvPercentage = 0.05);
    ~FatFingerCheck() override = default;
    
    bool validate(const Order& order, std::string& reason) const override;
    std::string_view getCheckName() const override { return "FatFingerCheck"; }
    
    // Set ADV (Average Daily Volume) for a symbol
    void setADV(std::string_view symbol, double adv);

private:
    double maxAdvPercentage_;
    mutable std::unordered_map<std::string, double> advMap_;
};

// Drawdown check - prevents trading when losses are too high
class DrawdownCheck : public IRiskCheck {
public:
    explicit DrawdownCheck(double maxDrawdownPercentage = 0.05);
    ~DrawdownCheck() override = default;
    
    bool validate(const Order& order, std::string& reason) const override;
    std::string_view getCheckName() const override { return "DrawdownCheck"; }
    
    void updatePnL(double currentPnL);
    void updateStartOfDayNAV(double nav);

private:
    double maxDrawdownPercentage_;
    mutable double startOfDayNAV_{0};
    mutable double currentPnL_{0};
};

// Concentration check - prevents over-concentration in single asset
class ConcentrationCheck : public IRiskCheck {
public:
    explicit ConcentrationCheck(double maxConcentrationPercentage = 0.10);
    ~ConcentrationCheck() override = default;
    
    bool validate(const Order& order, std::string& reason) const override;
    std::string_view getCheckName() const override { return "ConcentrationCheck"; }
    
    void updatePosition(std::string_view symbol, double quantity, double value);
    void updateTotalNAV(double nav);

private:
    double maxConcentrationPercentage_;
    mutable std::unordered_map<std::string, double> positionValues_;
    mutable double totalNAV_{0};
};

// Risk check result
struct RiskCheckResult {
    bool approved;
    std::vector<ViolationType> violations;
    std::string reason;
    
    RiskCheckResult() : approved(true) {}
    
    void addViolation(ViolationType type, std::string_view msg) {
        approved = false;
        violations.push_back(type);
        if (!reason.empty()) {
            reason += "; ";
        }
        reason += msg;
    }
};

// Template class for risk check aggregation
template<typename TOrder>
class RiskCheckAggregator {
public:
    using CheckPtr = std::unique_ptr<IRiskCheck>;
    
    void addCheck(CheckPtr check) {
        checks_.push_back(std::move(check));
    }
    
    RiskCheckResult validateAll(const TOrder& order) const {
        RiskCheckResult result;
        std::string reason;
        
        for (const auto& check : checks_) {
            if (!check->isEnabled()) {
                continue;
            }
            
            if (!check->validate(reinterpret_cast<const Order&>(order), reason)) {
                result.addViolation(ViolationType::NONE, reason);
            }
        }
        
        return result;
    }
    
    size_t getCheckCount() const {
        return checks_.size();
    }

private:
    std::vector<CheckPtr> checks_;
};

} // namespace wq::risk
