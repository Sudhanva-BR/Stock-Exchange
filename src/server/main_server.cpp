#include "miniexchange/ExchangeService.h"
#include <crow.h>
#include <crow/middlewares/cors.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    // Parse command line arguments for port
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number. Using default 8080." << std::endl;
            port = 8080;
        }
    }

    std::cout << "Starting Mini Exchange Web Server on port " << port << "..." << std::endl;

    // Create exchange service
    auto exchangeService = std::make_unique<miniexchange::ExchangeService>();
    exchangeService->start();

    // Create Crow app with CORS middleware
    crow::App<crow::CORSHandler> app;

    // CORS middleware
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .headers("Access-Control-Allow-Origin", "*")
        .headers("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
        .headers("Access-Control-Allow-Headers", "Content-Type, Authorization");

    // Handle OPTIONS requests for CORS preflight
    CROW_ROUTE(app, "/<path>")
        .methods("OPTIONS"_method)
    ([](const crow::request& req, crow::response& res, const std::string& /*path*/) {
        res.write("");
        res.end();
    });

    // ---------------------------------------------------------------------------
    // REST API Routes
    // ---------------------------------------------------------------------------

    // POST /api/orders - Submit an order
    CROW_ROUTE(app, "/api/orders")
        .methods("POST"_method)
    ([&exchangeService](const crow::request& req) {
        try {
            auto body = json::parse(req.body);
            
            std::string symbol = body["symbol"].get<std::string>();
            std::string side = body["side"].get<std::string>();
            std::string type = body["type"].get<std::string>();
            uint32_t quantity = body["quantity"].get<uint32_t>();
            
            std::optional<double> price;
            if (body.contains("price") && !body["price"].is_null()) {
                price = body["price"].get<double>();
            }

            uint64_t orderId = exchangeService->submitOrder(symbol, side, type, price, quantity);

            json response;
            response["orderId"] = orderId;
            response["status"] = "submitted";
            
            return crow::response(200, response.dump());
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            return crow::response(400, error.dump());
        }
    });

    // DELETE /api/orders/<symbol>/<orderId> - Cancel an order
    CROW_ROUTE(app, "/api/orders/<string>/<int>")
        .methods("DELETE"_method)
    ([&exchangeService](const std::string& symbol, int orderId) {
        bool success = exchangeService->cancelOrder(symbol, orderId);
        
        json response;
        response["success"] = success;
        if (!success) {
            response["error"] = "Order not found or symbol does not exist";
            return crow::response(404, response.dump());
        }
        return crow::response(200, response.dump());
    });

    // PUT /api/orders/<symbol>/<orderId> - Modify an order
    CROW_ROUTE(app, "/api/orders/<string>/<int>")
        .methods("PUT"_method)
    ([&exchangeService](const crow::request& req, const std::string& symbol, int orderId) {
        try {
            auto body = json::parse(req.body);
            
            double newPrice = body["price"].get<double>();
            uint32_t newQuantity = body["quantity"].get<uint32_t>();
            
            bool success = exchangeService->modifyOrder(symbol, orderId, newPrice, newQuantity);
            
            json response;
            response["success"] = success;
            if (!success) {
                response["error"] = "Order not found or symbol does not exist";
                return crow::response(404, response.dump());
            }
            return crow::response(200, response.dump());
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            return crow::response(400, error.dump());
        }
    });

    // GET /api/orderbook/<symbol> - Get order book snapshot
    CROW_ROUTE(app, "/api/orderbook/<string>")
        .methods("GET"_method)
    ([&exchangeService](const std::string& symbol) {
        std::string snapshot = exchangeService->getOrderBookSnapshot(symbol, 10);
        return crow::response(200, snapshot);
    });

    // GET /api/trades/<symbol>?limit=N - Get recent trades
    CROW_ROUTE(app, "/api/trades/<string>")
        .methods("GET"_method)
    ([&exchangeService](const crow::request& req, const std::string& symbol) {
        int limit = 50;
        if (req.url_params.get("limit")) {
            limit = std::stoi(req.url_params.get("limit"));
        }
        
        std::string trades = exchangeService->getRecentTrades(symbol, limit);
        return crow::response(200, trades);
    });

    // GET /api/symbols - Get list of active symbols
    CROW_ROUTE(app, "/api/symbols")
        .methods("GET"_method)
    ([&exchangeService]() {
        std::string symbols = exchangeService->getSymbols();
        return crow::response(200, symbols);
    });

    // GET /api/debug/orderbook/<symbol> - Get raw node / memory-pool debug data
    CROW_ROUTE(app, "/api/debug/orderbook/<string>")
        .methods("GET"_method)
    ([&exchangeService](const std::string& symbol) {
        std::string debug = exchangeService->getOrderBookDebug(symbol);
        crow::response res(200, debug);
        res.add_header("Content-Type", "application/json");
        return res;
    });


    // ---------------------------------------------------------------------------
    // WebSocket Route
    // ---------------------------------------------------------------------------

    CROW_ROUTE(app, "/ws")
        .websocket(&app)
        .onopen([&exchangeService](crow::websocket::connection& conn) {
            std::cout << "WebSocket connection opened" << std::endl;
            exchangeService->addWebSocketConnection(&conn);
        })
        .onclose([&exchangeService](crow::websocket::connection& conn, const std::string& reason) {
            std::cout << "WebSocket connection closed: " << reason << std::endl;
            exchangeService->removeWebSocketConnection(&conn);
        })
        .onmessage([&exchangeService](crow::websocket::connection& conn, const std::string& message, bool is_binary) {
            try {
                auto msg = json::parse(message);
                
                // Handle subscription messages
                if (msg.contains("subscribe")) {
                    std::string symbol = msg["subscribe"].get<std::string>();
                    std::cout << "Client subscribed to symbol: " << symbol << std::endl;
                    // For now, we broadcast all events to all clients
                    // In a production system, we'd filter by subscription
                }
            } catch (const std::exception& e) {
                std::cerr << "WebSocket message error: " << e.what() << std::endl;
            }
        });

    // ---------------------------------------------------------------------------
    // Start Server
    // ---------------------------------------------------------------------------

    std::cout << "Server running on http://localhost:" << port << std::endl;
    std::cout << "WebSocket endpoint: ws://localhost:" << port << "/ws" << std::endl;
    
    app.port(port).multithreaded().run();

    // Cleanup
    exchangeService->stop();

    return 0;
}
