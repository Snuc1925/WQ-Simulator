#!/usr/bin/env python3
"""
Market Data Generator - Simulates market data feed via UDP multicast

This script generates realistic market data for testing the Data Feed Handler.
Sends market ticks via UDP multicast to 239.255.0.1:12345
"""

import socket
import struct
import time
import random
import argparse
from typing import List, Tuple


class Symbol:
    """Represents a tradable symbol with price dynamics"""
    
    def __init__(self, symbol: str, base_price: float, volatility: float = 0.01):
        self.symbol = symbol
        self.price = base_price
        self.volatility = volatility
        self.bid_ask_spread = base_price * 0.001  # 0.1% spread
        
    def update_price(self):
        """Simulate price movement using random walk"""
        change = random.gauss(0, self.volatility * self.price)
        self.price = max(0.01, self.price + change)
        
    def get_bid_ask(self) -> Tuple[float, float]:
        """Get bid and ask prices"""
        bid = self.price - self.bid_ask_spread / 2
        ask = self.price + self.bid_ask_spread / 2
        return bid, ask


class MarketDataGenerator:
    """Generates and broadcasts market data"""
    
    def __init__(self, multicast_group: str = "239.255.0.1", port: int = 12345):
        self.multicast_group = multicast_group
        self.port = port
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
        
        # Initialize symbols
        self.symbols = self._create_symbols()
        
    def _create_symbols(self) -> List[Symbol]:
        """Create list of symbols to simulate"""
        symbols = [
            Symbol("AAPL", 150.0, 0.015),
            Symbol("GOOGL", 2800.0, 0.020),
            Symbol("MSFT", 300.0, 0.012),
            Symbol("AMZN", 3200.0, 0.018),
            Symbol("TSLA", 700.0, 0.030),
        ]
        return symbols
        
    def generate_tick(self, symbol: Symbol) -> bytes:
        """Generate market tick in NYSE format"""
        bid, ask = symbol.get_bid_ask()
        bid_size = random.randint(100, 10000)
        ask_size = random.randint(100, 10000)
        volume = random.randint(10000, 1000000)
        timestamp_ns = int(time.time() * 1e9)
        
        data = struct.pack(
            'dddqqqq16s',
            bid, ask, symbol.price,
            bid_size, ask_size, volume, timestamp_ns,
            symbol.symbol.encode('utf-8').ljust(16, b'\x00')
        )
        return data
        
    def run(self, ticks_per_second: int = 10, duration_seconds: int = 60):
        """Run the market data generator"""
        print(f"Starting market data generator")
        print(f"Multicast: {self.multicast_group}:{self.port}")
        print(f"Rate: {ticks_per_second} ticks/second\n")
        
        tick_interval = 1.0 / ticks_per_second
        start_time = time.time()
        tick_count = 0
        
        try:
            while True:
                if duration_seconds > 0 and time.time() - start_time >= duration_seconds:
                    break
                
                symbol = random.choice(self.symbols)
                symbol.update_price()
                tick_data = self.generate_tick(symbol)
                self.socket.sendto(tick_data, (self.multicast_group, self.port))
                
                tick_count += 1
                if tick_count % 100 == 0:
                    print(f"Sent {tick_count} ticks - Last: {symbol.symbol} @ ${symbol.price:.2f}")
                
                time.sleep(tick_interval)
                
        except KeyboardInterrupt:
            print("\nStopping generator...")
        finally:
            elapsed = time.time() - start_time
            print(f"\nTotal ticks sent: {tick_count}")
            print(f"Average rate: {tick_count / elapsed:.1f} ticks/sec")
            self.socket.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Market Data Generator')
    parser.add_argument('--rate', type=int, default=10, help='Ticks per second')
    parser.add_argument('--duration', type=int, default=0, help='Duration in seconds (0 = infinite)')
    args = parser.parse_args()
    
    generator = MarketDataGenerator()
    generator.run(args.rate, args.duration)
