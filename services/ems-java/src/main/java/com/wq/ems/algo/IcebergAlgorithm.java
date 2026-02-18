package com.wq.ems.algo;

import com.wq.ems.model.Order;
import java.util.ArrayList;
import java.util.List;

/**
 * Iceberg order algorithm
 * Shows only a small visible portion of the order at a time
 */
public class IcebergAlgorithm {
    
    /**
     * Generate iceberg child orders
     * 
     * @param parentOrder The parent order
     * @param visibleSize Size of each visible chunk
     * @return List of child orders
     */
    public List<Order> generateChildOrders(Order parentOrder, double visibleSize) {
        List<Order> childOrders = new ArrayList<>();
        
        double remainingQty = parentOrder.getQuantity();
        
        while (remainingQty > 0) {
            Order childOrder = new Order();
            childOrder.setSymbol(parentOrder.getSymbol());
            childOrder.setSide(parentOrder.getSide());
            childOrder.setType(Order.Type.LIMIT);
            childOrder.setPrice(parentOrder.getPrice());
            
            double thisOrderSize = Math.min(visibleSize, remainingQty);
            childOrder.setQuantity(thisOrderSize);
            childOrder.setStatus(Order.Status.PENDING_SUBMIT);
            
            childOrders.add(childOrder);
            remainingQty -= thisOrderSize;
        }
        
        return childOrders;
    }
    
    /**
     * Determine if next chunk should be released
     * 
     * @param currentOrder Current visible order
     * @return true if next chunk should be released
     */
    public boolean shouldReleaseNextChunk(Order currentOrder) {
        // Release next chunk when current is fully filled
        return currentOrder.isFilled();
    }
}
