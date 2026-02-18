#include "data_feed_handler.hpp"
#include "data_types.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "=== WQ Data Feed Handler ===" << std::endl;
    std::cout << "Starting service..." << std::endl;
    
    using namespace wq::datafeed;
    
    // Create data feed handler
    auto handler = DataFeedHandlerFactory::createHandler("239.255.0.1", 12345);
    
    // Register normalizers
    handler->registerNormalizer(Exchange::NYSE, 
        std::make_shared<NYSENormalizer>());
    handler->registerNormalizer(Exchange::NASDAQ, 
        std::make_shared<NASDAQNormalizer>());
    
    // Register callback using lambda
    handler->registerCallback([](const MarketData& data) {
        std::cout << "Market Data: " 
                  << data.symbol << " "
                  << "Bid=" << data.bidPrice << " "
                  << "Ask=" << data.askPrice << " "
                  << "Last=" << data.lastPrice << " "
                  << "Exchange=" << exchangeToString(data.exchange) << std::endl;
    });
    
    // Start listening
    if (!handler->start()) {
        std::cerr << "Failed to start handler" << std::endl;
        return 1;
    }
    
    std::cout << "Service started successfully" << std::endl;
    std::cout << "Listening for market data on multicast 239.255.0.1:12345" << std::endl;
    
    // Main loop
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Print statistics
        int64_t received, processed;
        handler->getStats(received, processed);
        
        if (received > 0) {
            std::cout << "Stats: Received=" << received 
                      << ", Processed=" << processed << std::endl;
        }
    }
    
    // Cleanup
    handler->stop();
    std::cout << "Service stopped" << std::endl;
    
    return 0;
}
