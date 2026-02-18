#pragma once

#include "alpha_strategy.hpp"
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>

namespace wq::alpha {

// Market data compatible structure
struct MarketData {
    std::string symbol;
    double price;
    int64_t volume;
    int64_t timestampNs;
    
    MarketData() = default;
    MarketData(MarketData&&) noexcept = default;
    MarketData& operator=(MarketData&&) noexcept = default;
};

// Thread pool for running alphas concurrently
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();
    
    // Enqueue task with lambda support
    template<typename Func>
    void enqueue(Func&& task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            tasks_.emplace(std::forward<Func>(task));
        }
        condition_.notify_one();
    }
    
    // Deleted copy/move
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    void stop();
    bool isStopped() const { return stop_.load(); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    
    void workerThread();
};

// Signal callback type
using SignalCallback = std::function<void(AlphaSignal&&)>;

// Alpha Engine Pool - manages thousands of alphas
class AlphaEnginePool {
public:
    explicit AlphaEnginePool(size_t numThreads = 8);
    ~AlphaEnginePool();
    
    // Add alpha to the pool
    void addAlpha(std::unique_ptr<IAlphaStrategy> alpha);
    
    // Load alphas from plugin directory
    bool loadPlugins(std::string_view pluginDir);
    
    // Process market data through all alphas
    void processMarketData(const MarketData& data);
    
    // Register callback for signals
    void registerSignalCallback(SignalCallback callback);
    
    // Get statistics - demonstrating different parameter passing
    void getStats(size_t& numAlphas, size_t& numSignals) const;  // By reference
    void getStats(size_t* numAlphas, size_t* numSignals) const;  // By pointer
    
    // Start and stop processing
    void start();
    void stop();

private:
    std::unique_ptr<ThreadPool> threadPool_;
    std::vector<std::unique_ptr<IAlphaStrategy>> alphas_;
    std::vector<SignalCallback> signalCallbacks_;
    
    mutable std::mutex alphasMutex_;
    mutable std::atomic<size_t> numSignalsGenerated_{0};
    std::atomic<bool> running_{false};
    
    // Process single alpha
    void processAlpha(IAlphaStrategy* alpha, const MarketData& data);
    
    // Notify callbacks
    void notifyCallbacks(AlphaSignal&& signal);
    
    // Load single plugin
    bool loadPlugin(const std::string& pluginPath);
    
    // Dynamically allocated plugin handles
    std::vector<void*> pluginHandles_;
};

// Factory for creating specific alpha types
class AlphaFactory {
public:
    // Function template for creating alphas
    template<typename TAlpha, typename... Args>
    static std::unique_ptr<IAlphaStrategy> create(Args&&... args) {
        return std::make_unique<TAlpha>(std::forward<Args>(args)...);
    }
    
    // Create alpha by name (demonstrates function overloading)
    static std::unique_ptr<IAlphaStrategy> create(std::string_view alphaType, std::string_view alphaId);
    static std::unique_ptr<IAlphaStrategy> create(std::string_view alphaType, std::string_view alphaId, int param);
};

// RAII wrapper for plugin loading
class PluginLoader {
public:
    explicit PluginLoader(std::string_view path);
    ~PluginLoader();
    
    // Move only
    PluginLoader(PluginLoader&& other) noexcept;
    PluginLoader& operator=(PluginLoader&& other) noexcept;
    
    // Deleted copy
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;
    
    bool isLoaded() const { return handle_ != nullptr; }
    void* getHandle() const { return handle_; }
    
    // Get symbol with type deduction
    template<typename T>
    T getSymbol(std::string_view symbolName) const {
        if (!handle_) return nullptr;
        return reinterpret_cast<T>(getSymbolImpl(symbolName));
    }

private:
    void* handle_{nullptr};
    std::string path_;
    
    void* getSymbolImpl(std::string_view symbolName) const;
};

} // namespace wq::alpha
