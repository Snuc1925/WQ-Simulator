package com.wq.ems;

import com.wq.ems.service.DatabaseService;
import com.wq.ems.service.OrderManagementService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.sql.SQLException;

/**
 * Main EMS Application
 */
public class EMSApplication {
    private static final Logger logger = LoggerFactory.getLogger(EMSApplication.class);
    
    private DatabaseService databaseService;
    private OrderManagementService orderManagementService;

    public static void main(String[] args) {
        EMSApplication app = new EMSApplication();
        
        try {
            app.initialize();
            app.run();
            
            // Keep application running
            Thread.currentThread().join();
            
        } catch (Exception e) {
            logger.error("Application error", e);
            System.exit(1);
        }
    }

    private void initialize() throws SQLException {
        logger.info("Initializing EMS Application");
        
        // Initialize database
        String dbHost = System.getenv().getOrDefault("DB_HOST", "localhost");
        int dbPort = Integer.parseInt(System.getenv().getOrDefault("DB_PORT", "5432"));
        String dbName = System.getenv().getOrDefault("DB_NAME", "wq_ems");
        String dbUser = System.getenv().getOrDefault("DB_USER", "postgres");
        String dbPassword = System.getenv().getOrDefault("DB_PASSWORD", "postgres");
        
        databaseService = new DatabaseService(dbHost, dbPort, dbName, dbUser, dbPassword);
        databaseService.initialize();
        
        // Initialize order management service
        orderManagementService = new OrderManagementService(databaseService);
        
        logger.info("EMS Application initialized successfully");
    }

    private void run() {
        logger.info("EMS Application running");
        
        // In a real system, this would:
        // 1. Start gRPC server to receive target portfolios
        // 2. Start FIX engine to communicate with exchanges
        // 3. Monitor order status and handle execution reports
    }

    private void shutdown() {
        logger.info("Shutting down EMS Application");
        
        if (orderManagementService != null) {
            orderManagementService.shutdown();
        }
        
        if (databaseService != null) {
            databaseService.close();
        }
    }
}
