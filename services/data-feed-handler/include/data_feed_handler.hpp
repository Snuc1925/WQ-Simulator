#pragma once

#include "data_types.hpp"
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>

namespace wq::datafeed {

// Forward declarations
class DataFeedHandler;

// Type alias for callback function pointer
using DataCallback = std::function<void(const MarketData&)>;

// Function pointer type for legacy compatibility
typedef void (*RawDataCallback)(const uint8_t*, size_t);

// Smart pointer factory class demonstrating friend
class DataFeedHandlerFactory {
private:
    friend class DataFeedHandler;
    
    static std::unique_ptr<DataFeedHandler> createHandler(
        std::string_view multicastGroup,
        uint16_t port);
};

// Main data feed handler class
class DataFeedHandler {
public:
    // Deleted copy constructor and assignment (move-only class)
    DataFeedHandler(const DataFeedHandler&) = delete;
    DataFeedHandler& operator=(const DataFeedHandler&) = delete;
    
    // Move constructor and assignment
    DataFeedHandler(DataFeedHandler&& other) noexcept;
    DataFeedHandler& operator=(DataFeedHandler&& other) noexcept;
    
    ~DataFeedHandler();
    
    // Start listening for data
    bool start();
    
    // Stop listening
    void stop();
    
    // Register callback with lambda support
    void registerCallback(DataCallback callback);
    
    // Function overloading - different signatures
    void registerCallback(RawDataCallback callback);
    
    // Register normalizer by exchange
    void registerNormalizer(Exchange exchange, std::shared_ptr<DataNormalizer> normalizer);
    
    // Get statistics - pass by reference
    void getStats(int64_t& packetsReceived, int64_t& packetsProcessed) const;
    
    // Get statistics - pass by pointer
    void getStats(int64_t* packetsReceived, int64_t* packetsProcessed) const;
    
    // Type deduction with const reference
    template<typename T>
    auto processData(const T& data) -> decltype(auto) {
        return processDataImpl(data);
    }
    
private:
    // Private constructor - only factory can create
    explicit DataFeedHandler(std::string multicastGroup, uint16_t port);
    
    friend class DataFeedHandlerFactory;
    
    // Implementation helper
    template<typename T>
    const T& processDataImpl(const T& data) {
        return data;
    }
    
    // Member variables with smart pointers
    std::string multicastGroup_;
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> listenerThread_;
    std::vector<DataCallback> callbacks_;
    std::vector<std::weak_ptr<DataNormalizer>> normalizers_;
    
    // Statistics
    mutable std::atomic<int64_t> packetsReceived_{0};
    mutable std::atomic<int64_t> packetsProcessed_{0};
    
    // Listener loop
    void listenerLoop();
    
    // Process received packet
    void processPacket(const uint8_t* data, size_t length);
};

// Optional wrapper for market data
struct MarketDataResult {
    std::optional<MarketData> data;
    std::string errorMessage;
    
    explicit operator bool() const {
        return data.has_value();
    }
};

// Constexpr configuration
namespace Config {
    constexpr size_t MAX_PACKET_SIZE = 65536;
    constexpr size_t BUFFER_SIZE = 1024 * 1024;  // 1MB
    constexpr int MAX_NORMALIZERS = 16;
}

} // namespace wq::datafeed
