package com.wq.ems.service;

import com.wq.ems.algo.IcebergAlgorithm;
import com.wq.ems.algo.TWAPAlgorithm;
import com.wq.ems.model.Order;
import com.wq.ems.model.TargetPosition;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;
import java.util.Map;
import java.util.concurrent.*;

/**
 * Order Management System
 * Handles order lifecycle and execution algorithms
 */
public class OrderManagementService {
    private static final Logger logger = LoggerFactory.getLogger(OrderManagementService.class);
    
    private final DatabaseService databaseService;
    private final TWAPAlgorithm twapAlgorithm;
    private final IcebergAlgorithm icebergAlgorithm;
    private final ScheduledExecutorService scheduler;
    private final Map<String, Order> activeOrders;

    public OrderManagementService(DatabaseService databaseService) {
        this.databaseService = databaseService;
        this.twapAlgorithm = new TWAPAlgorithm();
        this.icebergAlgorithm = new IcebergAlgorithm();
        this.scheduler = Executors.newScheduledThreadPool(10);
        this.activeOrders = new ConcurrentHashMap<>();
    }

    /**
     * Process target portfolio and generate orders
     */
    public void processTargetPortfolio(List<TargetPosition> positions) {
        for (TargetPosition position : positions) {
            double delta = position.getDelta();
            
            if (Math.abs(delta) < 0.001) {
                continue;  // No change needed
            }
            
            Order order = createOrderFromDelta(position.getSymbol(), delta);
            submitOrder(order);
        }
    }

    /**
     * Create order from position delta
     */
    private Order createOrderFromDelta(String symbol, double delta) {
        Order order = new Order();
        order.setSymbol(symbol);
        order.setQuantity(Math.abs(delta));
        order.setSide(delta > 0 ? Order.Side.BUY : Order.Side.SELL);
        
        // Decide algorithm based on order size
        if (Math.abs(delta) > 1000) {
            // Large order - use TWAP
            order.setType(Order.Type.TWAP);
            order.setTwapSlices(10);
            order.setTwapDurationMinutes(5);
        } else if (Math.abs(delta) > 500) {
            // Medium order - use Iceberg
            order.setType(Order.Type.ICEBERG);
            order.setIcebergVisibleSize(100.0);
        } else {
            // Small order - market order
            order.setType(Order.Type.MARKET);
        }
        
        return order;
    }

    /**
     * Submit order for execution
     */
    public void submitOrder(Order order) {
        try {
            order.setStatus(Order.Status.PENDING_SUBMIT);
            databaseService.saveOrder(order);
            activeOrders.put(order.getOrderId(), order);
            
            switch (order.getType()) {
                case TWAP:
                    executeTWAP(order);
                    break;
                case ICEBERG:
                    executeIceberg(order);
                    break;
                case MARKET:
                case LIMIT:
                    executeSimple(order);
                    break;
            }
            
            logger.info("Order submitted: {}", order);
        } catch (Exception e) {
            logger.error("Failed to submit order: {}", order, e);
            order.setStatus(Order.Status.REJECTED);
        }
    }

    /**
     * Execute TWAP order
     */
    private void executeTWAP(Order parentOrder) {
        List<Order> childOrders = twapAlgorithm.generateChildOrders(
            parentOrder,
            parentOrder.getTwapSlices(),
            parentOrder.getTwapDurationMinutes()
        );
        
        long delay = twapAlgorithm.calculateDelayBetweenSlices(
            parentOrder.getTwapDurationMinutes(),
            parentOrder.getTwapSlices()
        );
        
        // Schedule child orders
        for (int i = 0; i < childOrders.size(); i++) {
            final Order childOrder = childOrders.get(i);
            long scheduleDelay = i * delay;
            
            scheduler.schedule(() -> {
                executeSimple(childOrder);
                updateParentOrderFill(parentOrder, childOrder);
            }, scheduleDelay, TimeUnit.MILLISECONDS);
        }
        
        parentOrder.setStatus(Order.Status.SUBMITTED);
    }

    /**
     * Execute Iceberg order
     */
    private void executeIceberg(Order parentOrder) {
        List<Order> childOrders = icebergAlgorithm.generateChildOrders(
            parentOrder,
            parentOrder.getIcebergVisibleSize()
        );
        
        // Submit first chunk
        if (!childOrders.isEmpty()) {
            Order firstChunk = childOrders.get(0);
            executeSimple(firstChunk);
            
            // Monitor and submit subsequent chunks
            scheduleIcebergMonitoring(parentOrder, childOrders, 0);
        }
        
        parentOrder.setStatus(Order.Status.SUBMITTED);
    }

    /**
     * Monitor iceberg order and release next chunk when ready
     */
    private void scheduleIcebergMonitoring(Order parentOrder, List<Order> chunks, int currentIndex) {
        if (currentIndex >= chunks.size() - 1) {
            return;  // All chunks submitted
        }
        
        scheduler.scheduleAtFixedRate(() -> {
            Order currentChunk = chunks.get(currentIndex);
            
            if (icebergAlgorithm.shouldReleaseNextChunk(currentChunk)) {
                Order nextChunk = chunks.get(currentIndex + 1);
                executeSimple(nextChunk);
                updateParentOrderFill(parentOrder, currentChunk);
                
                // Continue monitoring next chunk
                scheduleIcebergMonitoring(parentOrder, chunks, currentIndex + 1);
            }
        }, 0, 1, TimeUnit.SECONDS);
    }

    /**
     * Execute simple market/limit order
     */
    private void executeSimple(Order order) {
        try {
            // Simulate order execution (in real system, send to exchange via FIX)
            order.setStatus(Order.Status.SUBMITTED);
            databaseService.saveOrder(order);
            
            // Simulate fill after short delay
            scheduler.schedule(() -> {
                simulateFill(order);
            }, 100, TimeUnit.MILLISECONDS);
            
        } catch (Exception e) {
            logger.error("Failed to execute order: {}", order, e);
        }
    }

    /**
     * Simulate order fill (in real system, this would come from FIX execution reports)
     */
    private void simulateFill(Order order) {
        try {
            double fillPrice = order.getPrice() > 0 ? order.getPrice() : 100.0;  // Simplified
            double fillQty = order.getRemainingQuantity();
            
            order.setFilledQuantity(order.getFilledQuantity() + fillQty);
            order.setStatus(Order.Status.FILLED);
            
            databaseService.saveOrder(order);
            databaseService.recordExecution(order.getOrderId(), order.getSymbol(), fillQty, fillPrice);
            
            // Update position
            double positionChange = order.getSide() == Order.Side.BUY ? fillQty : -fillQty;
            databaseService.updatePosition(order.getSymbol(), positionChange, fillPrice);
            
            activeOrders.remove(order.getOrderId());
            
            logger.info("Order filled: {}", order);
        } catch (Exception e) {
            logger.error("Failed to process fill for order: {}", order, e);
        }
    }

    /**
     * Update parent order fill status
     */
    private void updateParentOrderFill(Order parentOrder, Order childOrder) {
        try {
            parentOrder.setFilledQuantity(
                parentOrder.getFilledQuantity() + childOrder.getFilledQuantity()
            );
            
            if (parentOrder.isFilled()) {
                parentOrder.setStatus(Order.Status.FILLED);
                activeOrders.remove(parentOrder.getOrderId());
            } else {
                parentOrder.setStatus(Order.Status.PARTIALLY_FILLED);
            }
            
            databaseService.saveOrder(parentOrder);
        } catch (Exception e) {
            logger.error("Failed to update parent order: {}", parentOrder, e);
        }
    }

    public void shutdown() {
        scheduler.shutdown();
        try {
            if (!scheduler.awaitTermination(60, TimeUnit.SECONDS)) {
                scheduler.shutdownNow();
            }
        } catch (InterruptedException e) {
            scheduler.shutdownNow();
        }
    }
}
