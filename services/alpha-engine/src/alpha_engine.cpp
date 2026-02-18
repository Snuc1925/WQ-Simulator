#include "alpha_engine.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace wq::alpha {

// ThreadPool implementation
ThreadPool::ThreadPool(size_t numThreads) {
    workers_.reserve(numThreads);
    
    for (size_t i = 0; i < numThreads; ++i) {
        // Use lambda to capture this
        workers_.emplace_back([this]() {
            this->workerThread();
        });
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::stop() {
    if (stop_.exchange(true)) {
        return;  // Already stopped
    }
    
    condition_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerThread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            condition_.wait(lock, [this]() {
                return stop_.load() || !tasks_.empty();
            });
            
            if (stop_.load() && tasks_.empty()) {
                return;
            }
            
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }
        
        if (task) {
            task();
        }
    }
}

// AlphaEnginePool implementation
AlphaEnginePool::AlphaEnginePool(size_t numThreads)
    : threadPool_(std::make_unique<ThreadPool>(numThreads)) {
}

AlphaEnginePool::~AlphaEnginePool() {
    stop();
    
    // Unload plugins using dynamic memory management
    for (void* handle : pluginHandles_) {
        if (handle) {
            dlclose(handle);
        }
    }
}

void AlphaEnginePool::addAlpha(std::unique_ptr<IAlphaStrategy> alpha) {
    std::lock_guard<std::mutex> lock(alphasMutex_);
    alpha->initialize();
    alphas_.push_back(std::move(alpha));
}

void AlphaEnginePool::start() {
    running_.store(true);
}

void AlphaEnginePool::stop() {
    running_.store(false);
    threadPool_->stop();
}

void AlphaEnginePool::processMarketData(const MarketData& data) {
    if (!running_.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(alphasMutex_);
    
    // Process each alpha in thread pool using lambda with move semantics
    for (auto& alpha : alphas_) {
        if (!alpha->isActive()) {
            continue;
        }
        
        // Capture raw pointer for thread safety
        IAlphaStrategy* alphaPtr = alpha.get();
        
        // Enqueue task with lambda
        threadPool_->enqueue([this, alphaPtr, data]() {
            this->processAlpha(alphaPtr, data);
        });
    }
}

void AlphaEnginePool::processAlpha(IAlphaStrategy* alpha, const MarketData& data) {
    auto signal = alpha->onMarketData(data);
    
    if (signal.has_value()) {
        numSignalsGenerated_++;
        notifyCallbacks(std::move(signal.value()));
    }
}

void AlphaEnginePool::registerSignalCallback(SignalCallback callback) {
    signalCallbacks_.push_back(std::move(callback));
}

void AlphaEnginePool::notifyCallbacks(AlphaSignal&& signal) {
    // Use std::for_each with lambda
    std::for_each(signalCallbacks_.begin(), signalCallbacks_.end(),
        [&signal](const SignalCallback& callback) {
            // Create copy for each callback
            AlphaSignal signalCopy;
            signalCopy.alphaId = signal.alphaId;
            signalCopy.symbol = signal.symbol;
            signalCopy.signal = signal.signal;
            signalCopy.confidence = signal.confidence;
            signalCopy.timestampNs = signal.timestampNs;
            callback(std::move(signalCopy));
        });
}

// Pass by reference
void AlphaEnginePool::getStats(size_t& numAlphas, size_t& numSignals) const {
    std::lock_guard<std::mutex> lock(alphasMutex_);
    numAlphas = alphas_.size();
    numSignals = numSignalsGenerated_.load();
}

// Pass by pointer
void AlphaEnginePool::getStats(size_t* numAlphas, size_t* numSignals) const {
    std::lock_guard<std::mutex> lock(alphasMutex_);
    if (numAlphas) {
        *numAlphas = alphas_.size();
    }
    if (numSignals) {
        *numSignals = numSignalsGenerated_.load();
    }
}

bool AlphaEnginePool::loadPlugins(std::string_view pluginDir) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(pluginDir)) {
        return false;
    }
    
    for (const auto& entry : fs::directory_iterator(pluginDir)) {
        if (entry.path().extension() == ".so") {
            loadPlugin(entry.path().string());
        }
    }
    
    return true;
}

bool AlphaEnginePool::loadPlugin(const std::string& pluginPath) {
    void* handle = dlopen(pluginPath.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load plugin: " << dlerror() << std::endl;
        return false;
    }
    
    // Get plugin interface using function pointer
    using GetInterfaceFunc = const AlphaPluginInterface* (*)();
    auto getInterface = reinterpret_cast<GetInterfaceFunc>(
        dlsym(handle, "getPluginInterface")
    );
    
    if (!getInterface) {
        std::cerr << "Plugin missing getPluginInterface" << std::endl;
        dlclose(handle);
        return false;
    }
    
    const AlphaPluginInterface* interface = getInterface();
    if (!interface || !interface->createAlpha) {
        std::cerr << "Invalid plugin interface" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // Create alpha using function pointer
    IAlphaStrategy* alpha = interface->createAlpha("{}");
    if (alpha) {
        addAlpha(std::unique_ptr<IAlphaStrategy>(alpha));
        pluginHandles_.push_back(handle);
        return true;
    }
    
    dlclose(handle);
    return false;
}

// AlphaFactory implementation
std::unique_ptr<IAlphaStrategy> AlphaFactory::create(
    std::string_view alphaType, 
    std::string_view alphaId) {
    
    if (alphaType == "MeanReversion") {
        return create<MeanReversionAlpha>(std::string(alphaId));
    } else if (alphaType == "Momentum") {
        return create<MomentumAlpha>(std::string(alphaId));
    }
    
    return nullptr;
}

std::unique_ptr<IAlphaStrategy> AlphaFactory::create(
    std::string_view alphaType,
    std::string_view alphaId,
    int param) {
    
    if (alphaType == "MeanReversion") {
        return create<MeanReversionAlpha>(std::string(alphaId), param);
    } else if (alphaType == "Momentum") {
        return create<MomentumAlpha>(std::string(alphaId), param);
    }
    
    return nullptr;
}

// PluginLoader implementation
PluginLoader::PluginLoader(std::string_view path)
    : path_(path) {
    handle_ = dlopen(path_.c_str(), RTLD_LAZY);
}

PluginLoader::~PluginLoader() {
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
}

PluginLoader::PluginLoader(PluginLoader&& other) noexcept
    : handle_(other.handle_)
    , path_(std::move(other.path_)) {
    other.handle_ = nullptr;
}

PluginLoader& PluginLoader::operator=(PluginLoader&& other) noexcept {
    if (this != &other) {
        if (handle_) {
            dlclose(handle_);
        }
        handle_ = other.handle_;
        path_ = std::move(other.path_);
        other.handle_ = nullptr;
    }
    return *this;
}

void* PluginLoader::getSymbolImpl(std::string_view symbolName) const {
    if (!handle_) return nullptr;
    return dlsym(handle_, std::string(symbolName).c_str());
}

} // namespace wq::alpha
