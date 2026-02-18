package com.wq.ems.algo;

import com.wq.ems.model.Order;
import java.util.ArrayList;
import java.util.List;

/**
 * Time-Weighted Average Price (TWAP) algorithm
 * Slices large orders into smaller chunks over time
 */
public class TWAPAlgorithm {
    
    /**
     * Generate TWAP child orders
     * 
     * @param parentOrder The parent order to slice
     * @param numSlices Number of slices
     * @param durationMinutes Duration over which to execute
     * @return List of child orders
     */
    public List<Order> generateChildOrders(Order parentOrder, int numSlices, int durationMinutes) {
        List<Order> childOrders = new ArrayList<>();
        
        double sliceSize = parentOrder.getQuantity() / numSlices;
        double remainder = parentOrder.getQuantity() % numSlices;
        
        for (int i = 0; i < numSlices; i++) {
            Order childOrder = new Order();
            childOrder.setSymbol(parentOrder.getSymbol());
            childOrder.setSide(parentOrder.getSide());
            childOrder.setType(Order.Type.MARKET);
            
            // Distribute remainder across first few slices
            double thisSliceSize = sliceSize;
            if (i < remainder) {
                thisSliceSize += 1;
            }
            
            childOrder.setQuantity(thisSliceSize);
            childOrder.setStatus(Order.Status.PENDING_SUBMIT);
            
            childOrders.add(childOrder);
        }
        
        return childOrders;
    }
    
    /**
     * Calculate delay between slices in milliseconds
     * 
     * @param durationMinutes Total duration
     * @param numSlices Number of slices
     * @return Delay in milliseconds
     */
    public long calculateDelayBetweenSlices(int durationMinutes, int numSlices) {
        long totalMillis = durationMinutes * 60 * 1000L;
        return totalMillis / numSlices;
    }
}
