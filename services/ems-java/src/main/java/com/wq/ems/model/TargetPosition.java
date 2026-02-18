package com.wq.ems.model;

import java.time.Instant;

/**
 * Represents a target position from the portfolio optimizer
 */
public class TargetPosition {
    private String symbol;
    private double targetQuantity;
    private double currentQuantity;
    private Instant timestamp;

    public TargetPosition() {
    }

    public TargetPosition(String symbol, double targetQuantity, double currentQuantity) {
        this.symbol = symbol;
        this.targetQuantity = targetQuantity;
        this.currentQuantity = currentQuantity;
        this.timestamp = Instant.now();
    }

    public String getSymbol() {
        return symbol;
    }

    public void setSymbol(String symbol) {
        this.symbol = symbol;
    }

    public double getTargetQuantity() {
        return targetQuantity;
    }

    public void setTargetQuantity(double targetQuantity) {
        this.targetQuantity = targetQuantity;
    }

    public double getCurrentQuantity() {
        return currentQuantity;
    }

    public void setCurrentQuantity(double currentQuantity) {
        this.currentQuantity = currentQuantity;
    }

    public Instant getTimestamp() {
        return timestamp;
    }

    public void setTimestamp(Instant timestamp) {
        this.timestamp = timestamp;
    }

    public double getDelta() {
        return targetQuantity - currentQuantity;
    }

    @Override
    public String toString() {
        return "TargetPosition{" +
                "symbol='" + symbol + '\'' +
                ", targetQuantity=" + targetQuantity +
                ", currentQuantity=" + currentQuantity +
                ", delta=" + getDelta() +
                ", timestamp=" + timestamp +
                '}';
    }
}
