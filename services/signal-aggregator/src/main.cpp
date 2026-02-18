#include "signal_aggregator.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>

std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "=== WQ Signal Aggregator ===" << std::endl;
    std::cout << "Starting service..." << std::endl;
    
    using namespace wq::aggregator;
    
    // Create aggregator with weighted average strategy
    auto strategy = std::make_unique<WeightedAverageAggregation>();
    SignalAggregator aggregator(std::move(strategy));
    
    std::cout << "Service started successfully" << std::endl;
    std::cout << "Waiting for alpha signals..." << std::endl;
    
    // Simulate receiving signals
    int signalCount = 0;
    while (running) {
        // Simulate incoming signal
        AlphaSignal signal;
        signal.alphaId = "Alpha_" + std::to_string(signalCount % 10);
        signal.symbol = "AAPL";
        signal.signal = -0.5 + (std::rand() % 100) / 100.0;
        signal.confidence = 0.5 + (std::rand() % 50) / 100.0;
        signal.timestampNs = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
        
        aggregator.addSignal(std::move(signal));
        signalCount++;
        
        // Generate portfolio every 10 signals
        if (signalCount % 10 == 0) {
            auto portfolio = aggregator.generateTargetPortfolio();
            
            std::cout << "\n=== Target Portfolio ===" << std::endl;
            for (const auto& pos : portfolio) {
                std::cout << "Symbol: " << pos.symbol 
                          << ", Target: " << pos.targetQuantity << std::endl;
            }
            
            // Get aggregated signal
            auto aggSignal = aggregator.getAggregatedSignal("AAPL");
            if (aggSignal.has_value()) {
                std::cout << "Aggregated AAPL signal: " << aggSignal.value() << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Service stopped" << std::endl;
    return 0;
}
