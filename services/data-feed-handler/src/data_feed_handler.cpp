#include "data_feed_handler.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>

namespace wq::datafeed {

// DataFeedHandlerFactory implementation
std::unique_ptr<DataFeedHandler> DataFeedHandlerFactory::createHandler(
    std::string_view multicastGroup,
    uint16_t port) {
    // Use private constructor via friend access
    return std::unique_ptr<DataFeedHandler>(
        new DataFeedHandler(std::string(multicastGroup), port)
    );
}

// DataFeedHandler private constructor
DataFeedHandler::DataFeedHandler(std::string multicastGroup, uint16_t port)
    : multicastGroup_(std::move(multicastGroup))
    , port_(port) {
}

// Move constructor implementation (demonstrates move semantics)
DataFeedHandler::DataFeedHandler(DataFeedHandler&& other) noexcept
    : multicastGroup_(std::move(other.multicastGroup_))
    , port_(other.port_)
    , running_(other.running_.load())
    , listenerThread_(std::move(other.listenerThread_))
    , callbacks_(std::move(other.callbacks_))
    , normalizers_(std::move(other.normalizers_))
    , packetsReceived_(other.packetsReceived_.load())
    , packetsProcessed_(other.packetsProcessed_.load()) {
    other.running_ = false;
}

// Move assignment implementation
DataFeedHandler& DataFeedHandler::operator=(DataFeedHandler&& other) noexcept {
    if (this != &other) {
        stop();  // Stop current operation
        
        multicastGroup_ = std::move(other.multicastGroup_);
        port_ = other.port_;
        running_ = other.running_.load();
        listenerThread_ = std::move(other.listenerThread_);
        callbacks_ = std::move(other.callbacks_);
        normalizers_ = std::move(other.normalizers_);
        packetsReceived_ = other.packetsReceived_.load();
        packetsProcessed_ = other.packetsProcessed_.load();
        
        other.running_ = false;
    }
    return *this;
}

DataFeedHandler::~DataFeedHandler() {
    stop();
}

bool DataFeedHandler::start() {
    if (running_.exchange(true)) {
        return false;  // Already running
    }
    
    // Create listener thread using lambda
    listenerThread_ = std::make_unique<std::thread>([this]() {
        this->listenerLoop();
    });
    
    return true;
}

void DataFeedHandler::stop() {
    if (!running_.exchange(false)) {
        return;  // Not running
    }
    
    if (listenerThread_ && listenerThread_->joinable()) {
        listenerThread_->join();
    }
    listenerThread_.reset();
}

// Function overloading - std::function version
void DataFeedHandler::registerCallback(DataCallback callback) {
    callbacks_.push_back(std::move(callback));
}

// Function overloading - function pointer version
void DataFeedHandler::registerCallback(RawDataCallback callback) {
    // Wrap raw callback in lambda
    callbacks_.push_back([callback](const MarketData& data) {
        // This is a simplified wrapper
        callback(reinterpret_cast<const uint8_t*>(&data), sizeof(data));
    });
}

void DataFeedHandler::registerNormalizer(Exchange exchange, std::shared_ptr<DataNormalizer> normalizer) {
    normalizers_.push_back(normalizer);  // Store weak_ptr
}

// Pass by reference
void DataFeedHandler::getStats(int64_t& packetsReceived, int64_t& packetsProcessed) const {
    packetsReceived = packetsReceived_.load();
    packetsProcessed = packetsProcessed_.load();
}

// Pass by pointer
void DataFeedHandler::getStats(int64_t* packetsReceived, int64_t* packetsProcessed) const {
    if (packetsReceived) {
        *packetsReceived = packetsReceived_.load();
    }
    if (packetsProcessed) {
        *packetsProcessed = packetsProcessed_.load();
    }
}

void DataFeedHandler::listenerLoop() {
    // Create UDP socket for multicast
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket\n";
        return;
    }
    
    // Allow multiple sockets to bind to the same address
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Bind to the multicast port
    struct sockaddr_in localAddr;
    std::memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(port_);
    
    if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        std::cerr << "Failed to bind socket\n";
        close(sockfd);
        return;
    }
    
    // Join multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicastGroup_.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    
    std::vector<uint8_t> buffer(Config::MAX_PACKET_SIZE);
    
    while (running_.load()) {
        struct sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);
        
        ssize_t recvLen = recvfrom(sockfd, buffer.data(), buffer.size(), 0,
                                   (struct sockaddr*)&senderAddr, &senderLen);
        
        if (recvLen > 0) {
            packetsReceived_++;
            processPacket(buffer.data(), recvLen);
        }
    }
    
    close(sockfd);
}

void DataFeedHandler::processPacket(const uint8_t* data, size_t length) {
    // Try each normalizer (weak_ptr usage)
    for (auto& weakNormalizer : normalizers_) {
        if (auto normalizer = weakNormalizer.lock()) {  // Convert weak_ptr to shared_ptr
            auto result = normalizer->normalize(data, length);
            
            if (result.has_value()) {
                packetsProcessed_++;
                
                // Notify all callbacks using lambda
                std::for_each(callbacks_.begin(), callbacks_.end(),
                    [&result](const DataCallback& callback) {
                        callback(result.value());
                    });
                
                break;  // Successfully processed
            }
        }
    }
}

} // namespace wq::datafeed
