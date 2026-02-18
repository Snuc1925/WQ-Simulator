#include "data_types.hpp"
#include <cstring>
#include <chrono>

namespace wq::datafeed {

// NYSENormalizer implementation
std::optional<MarketData> NYSENormalizer::normalize(const uint8_t* rawData, size_t length) {
    if (length < 64) {
        return std::nullopt;
    }
    
    MarketData data;
    
    // Parse binary data using template function
    data.bidPrice = parseField<double>(rawData, 0);
    data.askPrice = parseField<double>(rawData, 8);
    data.lastPrice = parseField<double>(rawData, 16);
    data.bidSize = parseField<int64_t>(rawData, 24);
    data.askSize = parseField<int64_t>(rawData, 32);
    data.volume = parseField<int64_t>(rawData, 40);
    data.timestampNs = parseField<int64_t>(rawData, 48);
    
    // Parse symbol (null-terminated string at offset 56)
    char symbolBuf[16] = {0};
    std::memcpy(symbolBuf, rawData + 56, std::min(length - 56, sizeof(symbolBuf) - 1));
    data.symbol = symbolBuf;
    
    data.assetType = AssetType::EQUITY;
    data.exchange = Exchange::NYSE;
    
    if (!validate(data)) {
        return std::nullopt;
    }
    
    return data;
}

bool NYSENormalizer::validate(const MarketData& data) const {
    // Call base class validation
    if (!DataNormalizer::validate(data)) {
        return false;
    }
    
    // Additional NYSE-specific validations
    if (data.spread() > data.midPrice() * 0.1) {  // Spread > 10% is suspicious
        return false;
    }
    
    return true;
}

// NASDAQNormalizer implementation
std::optional<MarketData> NASDAQNormalizer::normalize(const uint8_t* rawData, size_t length) {
    if (length < 64) {
        return std::nullopt;
    }
    
    MarketData data;
    
    // NASDAQ has slightly different format
    data.lastPrice = parseField<double>(rawData, 0);
    data.bidPrice = parseField<double>(rawData, 8);
    data.askPrice = parseField<double>(rawData, 16);
    data.volume = parseField<int64_t>(rawData, 24);
    data.bidSize = parseField<int64_t>(rawData, 32);
    data.askSize = parseField<int64_t>(rawData, 40);
    data.timestampNs = parseField<int64_t>(rawData, 48);
    
    char symbolBuf[16] = {0};
    std::memcpy(symbolBuf, rawData + 56, std::min(length - 56, sizeof(symbolBuf) - 1));
    data.symbol = symbolBuf;
    
    data.assetType = AssetType::EQUITY;
    data.exchange = Exchange::NASDAQ;
    
    if (!validate(data)) {
        return std::nullopt;
    }
    
    return data;
}

} // namespace wq::datafeed
