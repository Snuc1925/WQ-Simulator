#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <vector>

namespace wq::datafeed {

// Enum class for asset types
enum class AssetType : uint8_t {
    EQUITY,
    FUTURE,
    OPTION,
    UNKNOWN
};

// Enum for exchange identifiers
enum class Exchange : uint8_t {
    NYSE,
    NASDAQ,
    CME,
    UNKNOWN
};

// Constexpr function to convert enum to string
constexpr std::string_view assetTypeToString(AssetType type) {
    switch (type) {
        case AssetType::EQUITY: return "EQUITY";
        case AssetType::FUTURE: return "FUTURE";
        case AssetType::OPTION: return "OPTION";
        default: return "UNKNOWN";
    }
}

constexpr std::string_view exchangeToString(Exchange exch) {
    switch (exch) {
        case Exchange::NYSE: return "NYSE";
        case Exchange::NASDAQ: return "NASDAQ";
        case Exchange::CME: return "CME";
        default: return "UNKNOWN";
    }
}

// Market data structure
struct MarketData {
    std::string symbol;
    double bidPrice;
    double askPrice;
    double lastPrice;
    int64_t bidSize;
    int64_t askSize;
    int64_t volume;
    int64_t timestampNs;
    AssetType assetType;
    Exchange exchange;
    
    // Default constructor
    MarketData() = default;
    
    // Move constructor
    MarketData(MarketData&& other) noexcept
        : symbol(std::move(other.symbol))
        , bidPrice(other.bidPrice)
        , askPrice(other.askPrice)
        , lastPrice(other.lastPrice)
        , bidSize(other.bidSize)
        , askSize(other.askSize)
        , volume(other.volume)
        , timestampNs(other.timestampNs)
        , assetType(other.assetType)
        , exchange(other.exchange) {}
    
    // Move assignment
    MarketData& operator=(MarketData&& other) noexcept {
        if (this != &other) {
            symbol = std::move(other.symbol);
            bidPrice = other.bidPrice;
            askPrice = other.askPrice;
            lastPrice = other.lastPrice;
            bidSize = other.bidSize;
            askSize = other.askSize;
            volume = other.volume;
            timestampNs = other.timestampNs;
            assetType = other.assetType;
            exchange = other.exchange;
        }
        return *this;
    }
    
    // Calculate mid price
    double midPrice() const {
        return (bidPrice + askPrice) / 2.0;
    }
    
    // Calculate spread
    double spread() const {
        return askPrice - bidPrice;
    }
};

// Abstract base class for data normalizers (demonstrates virtual functions)
class DataNormalizer {
public:
    virtual ~DataNormalizer() = default;  // Virtual destructor
    
    // Pure virtual function - makes this an abstract class
    virtual std::optional<MarketData> normalize(const uint8_t* rawData, size_t length) = 0;
    
    // Virtual function with default implementation
    virtual bool validate(const MarketData& data) const {
        return data.bidPrice > 0 && data.askPrice > 0 && data.askPrice >= data.bidPrice;
    }
    
    // Non-virtual function
    std::string getNormalizerType() const {
        return normalizerType_;
    }

protected:
    explicit DataNormalizer(std::string type) : normalizerType_(std::move(type)) {}
    std::string normalizerType_;
};

// Concrete normalizer for NYSE (demonstrates inheritance and vtable)
class NYSENormalizer : public DataNormalizer {
public:
    NYSENormalizer() : DataNormalizer("NYSE") {}
    
    std::optional<MarketData> normalize(const uint8_t* rawData, size_t length) override;
    bool validate(const MarketData& data) const override;
};

// Concrete normalizer for NASDAQ
class NASDAQNormalizer : public DataNormalizer {
public:
    NASDAQNormalizer() : DataNormalizer("NASDAQ") {}
    
    std::optional<MarketData> normalize(const uint8_t* rawData, size_t length) override;
};

// Template function for type-safe data parsing
template<typename T>
T parseField(const uint8_t* data, size_t offset) {
    T value;
    std::memcpy(&value, data + offset, sizeof(T));
    return value;
}

// Function template overloading
template<typename T>
void logValue(const T& value, std::string_view name);

template<>
inline void logValue<double>(const double& value, std::string_view name) {
    // Specialized for double
}

template<>
inline void logValue<int64_t>(const int64_t& value, std::string_view name) {
    // Specialized for int64_t
}

} // namespace wq::datafeed
