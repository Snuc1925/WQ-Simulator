package com.wq.ems.service;

import com.wq.ems.model.Order;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.sql.*;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

/**
 * PostgreSQL database service for persisting orders and executions
 */
public class DatabaseService {
    private static final Logger logger = LoggerFactory.getLogger(DatabaseService.class);
    
    private final String jdbcUrl;
    private final Properties props;
    private Connection connection;

    public DatabaseService(String host, int port, String database, String username, String password) {
        this.jdbcUrl = String.format("jdbc:postgresql://%s:%d/%s", host, port, database);
        this.props = new Properties();
        this.props.setProperty("user", username);
        this.props.setProperty("password", password);
    }

    /**
     * Initialize database connection and create tables
     */
    public void initialize() throws SQLException {
        connection = DriverManager.getConnection(jdbcUrl, props);
        createTables();
        logger.info("Database initialized successfully");
    }

    /**
     * Create necessary tables
     */
    private void createTables() throws SQLException {
        String createOrdersTable = """
            CREATE TABLE IF NOT EXISTS orders (
                order_id VARCHAR(255) PRIMARY KEY,
                symbol VARCHAR(50) NOT NULL,
                quantity DOUBLE PRECISION NOT NULL,
                filled_quantity DOUBLE PRECISION DEFAULT 0,
                side VARCHAR(10) NOT NULL,
                type VARCHAR(20) NOT NULL,
                status VARCHAR(20) NOT NULL,
                price DOUBLE PRECISION,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                twap_slices INTEGER,
                twap_duration_minutes INTEGER,
                iceberg_visible_size DOUBLE PRECISION
            )
        """;

        String createExecutionsTable = """
            CREATE TABLE IF NOT EXISTS executions (
                execution_id SERIAL PRIMARY KEY,
                order_id VARCHAR(255) NOT NULL,
                symbol VARCHAR(50) NOT NULL,
                quantity DOUBLE PRECISION NOT NULL,
                price DOUBLE PRECISION NOT NULL,
                executed_at TIMESTAMP NOT NULL,
                FOREIGN KEY (order_id) REFERENCES orders(order_id)
            )
        """;

        String createPositionsTable = """
            CREATE TABLE IF NOT EXISTS positions (
                symbol VARCHAR(50) PRIMARY KEY,
                quantity DOUBLE PRECISION NOT NULL,
                avg_cost DOUBLE PRECISION NOT NULL,
                last_updated TIMESTAMP NOT NULL
            )
        """;

        try (Statement stmt = connection.createStatement()) {
            stmt.execute(createOrdersTable);
            stmt.execute(createExecutionsTable);
            stmt.execute(createPositionsTable);
        }
    }

    /**
     * Save order to database
     */
    public void saveOrder(Order order) throws SQLException {
        String sql = """
            INSERT INTO orders (
                order_id, symbol, quantity, filled_quantity, side, type, status,
                price, created_at, updated_at, twap_slices, twap_duration_minutes, iceberg_visible_size
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT (order_id) DO UPDATE SET
                filled_quantity = EXCLUDED.filled_quantity,
                status = EXCLUDED.status,
                updated_at = EXCLUDED.updated_at
        """;

        try (PreparedStatement pstmt = connection.prepareStatement(sql)) {
            pstmt.setString(1, order.getOrderId());
            pstmt.setString(2, order.getSymbol());
            pstmt.setDouble(3, order.getQuantity());
            pstmt.setDouble(4, order.getFilledQuantity());
            pstmt.setString(5, order.getSide().name());
            pstmt.setString(6, order.getType().name());
            pstmt.setString(7, order.getStatus().name());
            pstmt.setDouble(8, order.getPrice());
            pstmt.setTimestamp(9, Timestamp.from(order.getCreatedAt()));
            pstmt.setTimestamp(10, Timestamp.from(order.getUpdatedAt()));
            
            if (order.getTwapSlices() != null) {
                pstmt.setInt(11, order.getTwapSlices());
            } else {
                pstmt.setNull(11, Types.INTEGER);
            }
            
            if (order.getTwapDurationMinutes() != null) {
                pstmt.setInt(12, order.getTwapDurationMinutes());
            } else {
                pstmt.setNull(12, Types.INTEGER);
            }
            
            if (order.getIcebergVisibleSize() != null) {
                pstmt.setDouble(13, order.getIcebergVisibleSize());
            } else {
                pstmt.setNull(13, Types.DOUBLE);
            }
            
            pstmt.executeUpdate();
        }
    }

    /**
     * Get order by ID
     */
    public Order getOrder(String orderId) throws SQLException {
        String sql = "SELECT * FROM orders WHERE order_id = ?";
        
        try (PreparedStatement pstmt = connection.prepareStatement(sql)) {
            pstmt.setString(1, orderId);
            
            try (ResultSet rs = pstmt.executeQuery()) {
                if (rs.next()) {
                    return mapResultSetToOrder(rs);
                }
            }
        }
        
        return null;
    }

    /**
     * Get all active orders
     */
    public List<Order> getActiveOrders() throws SQLException {
        String sql = "SELECT * FROM orders WHERE status IN ('NEW', 'PENDING_SUBMIT', 'SUBMITTED', 'PARTIALLY_FILLED')";
        List<Order> orders = new ArrayList<>();
        
        try (Statement stmt = connection.createStatement();
             ResultSet rs = stmt.executeQuery(sql)) {
            
            while (rs.next()) {
                orders.add(mapResultSetToOrder(rs));
            }
        }
        
        return orders;
    }

    /**
     * Record execution
     */
    public void recordExecution(String orderId, String symbol, double quantity, double price) throws SQLException {
        String sql = """
            INSERT INTO executions (order_id, symbol, quantity, price, executed_at)
            VALUES (?, ?, ?, ?, NOW())
        """;
        
        try (PreparedStatement pstmt = connection.prepareStatement(sql)) {
            pstmt.setString(1, orderId);
            pstmt.setString(2, symbol);
            pstmt.setDouble(3, quantity);
            pstmt.setDouble(4, price);
            pstmt.executeUpdate();
        }
    }

    /**
     * Update position
     */
    public void updatePosition(String symbol, double quantity, double avgCost) throws SQLException {
        String sql = """
            INSERT INTO positions (symbol, quantity, avg_cost, last_updated)
            VALUES (?, ?, ?, NOW())
            ON CONFLICT (symbol) DO UPDATE SET
                quantity = positions.quantity + EXCLUDED.quantity,
                avg_cost = (positions.quantity * positions.avg_cost + EXCLUDED.quantity * EXCLUDED.avg_cost) / 
                          (positions.quantity + EXCLUDED.quantity),
                last_updated = NOW()
        """;
        
        try (PreparedStatement pstmt = connection.prepareStatement(sql)) {
            pstmt.setString(1, symbol);
            pstmt.setDouble(2, quantity);
            pstmt.setDouble(3, avgCost);
            pstmt.executeUpdate();
        }
    }

    private Order mapResultSetToOrder(ResultSet rs) throws SQLException {
        Order order = new Order();
        order.setOrderId(rs.getString("order_id"));
        order.setSymbol(rs.getString("symbol"));
        order.setQuantity(rs.getDouble("quantity"));
        order.setFilledQuantity(rs.getDouble("filled_quantity"));
        order.setSide(Order.Side.valueOf(rs.getString("side")));
        order.setType(Order.Type.valueOf(rs.getString("type")));
        order.setStatus(Order.Status.valueOf(rs.getString("status")));
        order.setPrice(rs.getDouble("price"));
        
        Integer twapSlices = rs.getInt("twap_slices");
        if (!rs.wasNull()) {
            order.setTwapSlices(twapSlices);
        }
        
        Integer twapDuration = rs.getInt("twap_duration_minutes");
        if (!rs.wasNull()) {
            order.setTwapDurationMinutes(twapDuration);
        }
        
        Double icebergSize = rs.getDouble("iceberg_visible_size");
        if (!rs.wasNull()) {
            order.setIcebergVisibleSize(icebergSize);
        }
        
        return order;
    }

    public void close() {
        try {
            if (connection != null && !connection.isClosed()) {
                connection.close();
                logger.info("Database connection closed");
            }
        } catch (SQLException e) {
            logger.error("Error closing database connection", e);
        }
    }
}
