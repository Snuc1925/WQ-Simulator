#include "risk_guardian.hpp"
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
    
    std::cout << "=== WQ Risk Guardian ===" << std::endl;
    std::cout << "Starting service..." << std::endl;
    
    using namespace wq::risk;
    
    // Build risk guardian using builder pattern
    auto guardian = RiskGuardianBuilder()
        .withInitialNAV(1000000.0)
        .withFatFingerCheck(0.05)
        .withDrawdownCheck(0.05)
        .withConcentrationCheck(0.10)
        .build();
    
    std::cout << "Risk Guardian initialized with:" << std::endl;
    std::cout << "  - Initial NAV: $1,000,000" << std::endl;
    std::cout << "  - Fat Finger Check: 5% of ADV" << std::endl;
    std::cout << "  - Drawdown Limit: 5%" << std::endl;
    std::cout << "  - Concentration Limit: 10%" << std::endl;
    
    // Simulate order validation
    int orderCount = 0;
    while (running) {
        // Create test order
        Order order;
        order.orderId = "Order_" + std::to_string(orderCount++);
        order.symbol = "AAPL";
        order.quantity = 100 + (std::rand() % 500);
        order.side = (std::rand() % 2) ? OrderSide::BUY : OrderSide::SELL;
        order.price = 150.0 + (std::rand() % 100) / 10.0;
        order.timestampNs = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
        
        // Validate order
        auto startTime = std::chrono::high_resolution_clock::now();
        auto result = guardian->validateOrder(order);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        auto durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            endTime - startTime).count();
        
        std::cout << "\nOrder " << order.orderId << ": "
                  << order.symbol << " "
                  << (order.side == OrderSide::BUY ? "BUY" : "SELL") << " "
                  << order.quantity << " @ $" << order.price << std::endl;
        
        if (result.approved) {
            std::cout << "✓ APPROVED (validated in " << durationNs << "ns)" << std::endl;
            
            // Update position after approval
            double qtyChange = order.side == OrderSide::BUY ? 
                order.quantity : -order.quantity;
            guardian->updatePosition(order.symbol, qtyChange, order.price);
        } else {
            std::cout << "✗ REJECTED: " << result.reason << std::endl;
            std::cout << "  Violations: ";
            for (const auto& violation : result.violations) {
                std::cout << violationTypeToString(violation) << " ";
            }
            std::cout << std::endl;
        }
        
        // Print statistics
        if (orderCount % 10 == 0) {
            uint64_t validationCount = guardian->getValidationCount<uint64_t>();
            std::cout << "\n=== Statistics ===" << std::endl;
            std::cout << "Total validations: " << validationCount << std::endl;
            
            size_t numPositions;
            double totalExposure;
            guardian->getPositionManager()->getStats(numPositions, totalExposure);
            std::cout << "Active positions: " << numPositions << std::endl;
            std::cout << "Total exposure: $" << totalExposure << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    std::cout << "\nService stopped" << std::endl;
    return 0;
}
