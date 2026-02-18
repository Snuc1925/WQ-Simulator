#include "alpha_engine.hpp"
#include "alpha_strategy.hpp"
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
    
    std::cout << "=== WQ Alpha Engine Pool ===" << std::endl;
    std::cout << "Starting service..." << std::endl;
    
    using namespace wq::alpha;
    
    // Create alpha engine pool with 8 worker threads
    AlphaEnginePool engine(8);
    
    // Add sample alphas (in real system, load from plugins)
    std::cout << "Loading alpha strategies..." << std::endl;
    
    for (int i = 0; i < 100; i++) {
        std::string alphaId = "MeanReversion_" + std::to_string(i);
        auto alpha = AlphaFactory::create<MeanReversionAlpha>(alphaId, 20);
        engine.addAlpha(std::move(alpha));
    }
    
    for (int i = 0; i < 100; i++) {
        std::string alphaId = "Momentum_" + std::to_string(i);
        auto alpha = AlphaFactory::create<MomentumAlpha>(alphaId, 10);
        engine.addAlpha(std::move(alpha));
    }
    
    // Register signal callback using lambda
    engine.registerSignalCallback([](AlphaSignal&& signal) {
        std::cout << "Signal: " << signal.alphaId 
                  << " " << signal.symbol
                  << " signal=" << signal.signal
                  << " confidence=" << signal.confidence << std::endl;
    });
    
    // Start engine
    engine.start();
    
    size_t numAlphas, numSignals;
    engine.getStats(numAlphas, numSignals);
    std::cout << "Service started with " << numAlphas << " alphas" << std::endl;
    
    // Simulate market data feed
    std::cout << "Simulating market data..." << std::endl;
    
    int tickCount = 0;
    while (running) {
        // Create sample market data
        MarketData data;
        data.symbol = "AAPL";
        data.price = 150.0 + (std::rand() % 100) / 100.0;
        data.volume = 10000;
        data.timestampNs = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
        
        // Process through all alphas
        engine.processMarketData(data);
        
        tickCount++;
        if (tickCount % 10 == 0) {
            engine.getStats(numAlphas, numSignals);
            std::cout << "Processed " << tickCount << " ticks, "
                      << "Generated " << numSignals << " signals" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup
    engine.stop();
    std::cout << "Service stopped" << std::endl;
    
    return 0;
}
