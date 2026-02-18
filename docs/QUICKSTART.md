# Quick Start Guide

This guide will help you get the WQ-Simulator up and running quickly.

## Prerequisites

### C++ Services
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc
```

### Java EMS
```bash
sudo apt-get install -y openjdk-11-jdk maven
```

### Python Utilities
```bash
pip3 install grpcio grpcio-tools
```

### Database (Optional)
```bash
sudo apt-get install -y postgresql-14
```

## Building

### Build C++ Services
```bash
./scripts/build.sh
```

This will:
- Generate protobuf/gRPC code
- Compile all C++ services
- Create executables in `build/` directory

### Build Java EMS
```bash
./scripts/build-ems.sh
```

## Running the System

### Option 1: Run Individual Services

#### Terminal 1 - Start Databases
```bash
docker-compose up -d
```

#### Terminal 2 - Data Feed Handler
```bash
cd build
./services/data-feed-handler/data-feed-handler-server
```

#### Terminal 3 - Alpha Engine Pool
```bash
cd build
./services/alpha-engine/alpha-engine-server
```

#### Terminal 4 - Signal Aggregator
```bash
cd build
./services/signal-aggregator/signal-aggregator-server
```

#### Terminal 5 - Risk Guardian
```bash
cd build
./services/risk-guardian/risk-guardian-server
```

#### Terminal 6 - EMS (Java)
```bash
# Set up database first
export DB_HOST=localhost
export DB_PORT=5432
export DB_NAME=wq_ems
export DB_USER=postgres
export DB_PASSWORD=postgres

# Run EMS
java -jar services/ems-java/target/ems-java-1.0.0.jar
```

#### Terminal 7 - Market Data Generator
```bash
python3 scripts/market_data_generator.py --rate 10
```

### Option 2: Run Demo Services

The demo services work standalone without needing full integration:

```bash
# Data Feed Handler (listens for UDP multicast)
cd build
./services/data-feed-handler/data-feed-handler-server &

# Generate sample market data
python3 scripts/market_data_generator.py --rate 10 --duration 60 &

# Alpha Engine (processes market data)
./services/alpha-engine/alpha-engine-server &

# Signal Aggregator
./services/signal-aggregator/signal-aggregator-server &

# Risk Guardian
./services/risk-guardian/risk-guardian-server
```

## Verifying the System

### Check Data Feed Handler
You should see output like:
```
=== WQ Data Feed Handler ===
Starting service...
Service started successfully
Listening for market data on multicast 239.255.0.1:12345
Stats: Received=100, Processed=100
Market Data: AAPL Bid=149.95 Ask=150.05 Last=150.00 Exchange=NYSE
```

### Check Alpha Engine
You should see output like:
```
=== WQ Alpha Engine Pool ===
Starting service...
Service started with 200 alphas
Signal: MeanReversion_0 AAPL signal=0.234 confidence=0.678
Processed 10 ticks, Generated 1234 signals
```

### Check Risk Guardian
You should see output like:
```
=== WQ Risk Guardian ===
Order Order_1: AAPL BUY 150 @ $150.50
✓ APPROVED (validated in 12345ns)
=== Statistics ===
Total validations: 100
Active positions: 5
Total exposure: $75000.50
```

## Testing Individual Components

### Test Data Feed Handler
```bash
# Terminal 1: Start handler
cd build
./services/data-feed-handler/data-feed-handler-server

# Terminal 2: Send test data
python3 scripts/market_data_generator.py --rate 1 --duration 10
```

### Test Alpha Engine
```bash
cd build
./services/alpha-engine/alpha-engine-server
# The demo generates its own market data
```

### Test Risk Guardian
```bash
cd build
./services/risk-guardian/risk-guardian-server
# The demo generates and validates test orders
```

## Common Issues

### Issue: "protoc not found"
**Solution**: Install protobuf compiler
```bash
sudo apt-get install protobuf-compiler
```

### Issue: "libgrpc++ not found"
**Solution**: Install gRPC development libraries
```bash
sudo apt-get install libgrpc++-dev
```

### Issue: "Cannot connect to database"
**Solution**: Start PostgreSQL via Docker
```bash
docker-compose up -d postgres
```

### Issue: "Port already in use"
**Solution**: Kill existing processes
```bash
# Find process using port 12345
lsof -i :12345
kill -9 <PID>
```

## Next Steps

1. **Explore the Code**: Review `docs/CPP_FEATURES.md` to see all C++ features demonstrated
2. **Architecture**: Read `docs/ARCHITECTURE.md` for system design details
3. **Customize**: Modify alpha strategies in `services/alpha-engine/`
4. **Extend**: Add new risk checks in `services/risk-guardian/`

## Performance Tuning

### Increase Alpha Strategies
Edit `services/alpha-engine/src/main.cpp`:
```cpp
// Change from 100 to 1000
for (int i = 0; i < 1000; i++) {
    auto alpha = AlphaFactory::create<MeanReversionAlpha>(alphaId, 20);
    engine.addAlpha(std::move(alpha));
}
```

### Increase Thread Pool Size
Edit `services/alpha-engine/src/main.cpp`:
```cpp
// Change from 8 to 16 threads
AlphaEnginePool engine(16);
```

### Increase Market Data Rate
```bash
# Generate 100 ticks per second
python3 scripts/market_data_generator.py --rate 100
```

## Monitoring

### Watch System Resources
```bash
# CPU usage
top

# Memory usage
free -h

# Network traffic
iftop -i lo  # For multicast on loopback
```

### Check Logs
Services log to stdout. Redirect to files:
```bash
./services/data-feed-handler/data-feed-handler-server > data-feed.log 2>&1 &
./services/alpha-engine/alpha-engine-server > alpha-engine.log 2>&1 &
```

## Stopping the System

```bash
# Stop individual services with Ctrl+C

# Stop all background processes
pkill -f "data-feed-handler-server"
pkill -f "alpha-engine-server"
pkill -f "signal-aggregator-server"
pkill -f "risk-guardian-server"
pkill -f "java.*ems-java"

# Stop Docker services
docker-compose down
```
