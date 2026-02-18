package com.wq.ems.model;

import java.time.Instant;
import java.util.UUID;

/**
 * Represents an order in the EMS system
 */
public class Order {
    public enum Side {
        BUY, SELL
    }

    public enum Status {
        NEW,
        PENDING_SUBMIT,
        SUBMITTED,
        PARTIALLY_FILLED,
        FILLED,
        CANCELLED,
        REJECTED
    }

    public enum Type {
        MARKET,
        LIMIT,
        TWAP,
        ICEBERG
    }

    private String orderId;
    private String symbol;
    private double quantity;
    private double filledQuantity;
    private Side side;
    private Type type;
    private Status status;
    private double price;
    private Instant createdAt;
    private Instant updatedAt;

    // For TWAP orders
    private Integer twapSlices;
    private Integer twapDurationMinutes;

    // For Iceberg orders
    private Double icebergVisibleSize;

    public Order() {
        this.orderId = UUID.randomUUID().toString();
        this.createdAt = Instant.now();
        this.updatedAt = Instant.now();
        this.status = Status.NEW;
        this.filledQuantity = 0;
    }

    public Order(String symbol, double quantity, Side side, Type type) {
        this();
        this.symbol = symbol;
        this.quantity = quantity;
        this.side = side;
        this.type = type;
    }

    // Getters and setters
    public String getOrderId() {
        return orderId;
    }

    public void setOrderId(String orderId) {
        this.orderId = orderId;
    }

    public String getSymbol() {
        return symbol;
    }

    public void setSymbol(String symbol) {
        this.symbol = symbol;
    }

    public double getQuantity() {
        return quantity;
    }

    public void setQuantity(double quantity) {
        this.quantity = quantity;
    }

    public double getFilledQuantity() {
        return filledQuantity;
    }

    public void setFilledQuantity(double filledQuantity) {
        this.filledQuantity = filledQuantity;
        this.updatedAt = Instant.now();
    }

    public Side getSide() {
        return side;
    }

    public void setSide(Side side) {
        this.side = side;
    }

    public Type getType() {
        return type;
    }

    public void setType(Type type) {
        this.type = type;
    }

    public Status getStatus() {
        return status;
    }

    public void setStatus(Status status) {
        this.status = status;
        this.updatedAt = Instant.now();
    }

    public double getPrice() {
        return price;
    }

    public void setPrice(double price) {
        this.price = price;
    }

    public Instant getCreatedAt() {
        return createdAt;
    }

    public Instant getUpdatedAt() {
        return updatedAt;
    }

    public Integer getTwapSlices() {
        return twapSlices;
    }

    public void setTwapSlices(Integer twapSlices) {
        this.twapSlices = twapSlices;
    }

    public Integer getTwapDurationMinutes() {
        return twapDurationMinutes;
    }

    public void setTwapDurationMinutes(Integer twapDurationMinutes) {
        this.twapDurationMinutes = twapDurationMinutes;
    }

    public Double getIcebergVisibleSize() {
        return icebergVisibleSize;
    }

    public void setIcebergVisibleSize(Double icebergVisibleSize) {
        this.icebergVisibleSize = icebergVisibleSize;
    }

    public double getRemainingQuantity() {
        return quantity - filledQuantity;
    }

    public boolean isFilled() {
        return Math.abs(filledQuantity - quantity) < 0.001;
    }

    @Override
    public String toString() {
        return "Order{" +
                "orderId='" + orderId + '\'' +
                ", symbol='" + symbol + '\'' +
                ", quantity=" + quantity +
                ", filledQuantity=" + filledQuantity +
                ", side=" + side +
                ", type=" + type +
                ", status=" + status +
                ", price=" + price +
                '}';
    }
}
