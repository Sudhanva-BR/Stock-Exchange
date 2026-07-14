#include "miniexchange/CsvOrderReader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <string>

namespace miniexchange {

namespace {

// Trim leading and trailing whitespace in-place.
std::string& trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char c) { return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
    return s;
}

// Case-insensitive string comparison.
bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(),
        [](unsigned char x, unsigned char y) {
            return std::tolower(x) == std::tolower(y);
        });
}

// Split a single CSV row on commas. Does NOT handle quoted fields (not needed here).
std::vector<std::string> splitCsvRow(const std::string& row) {
    std::vector<std::string> fields;
    std::stringstream ss(row);
    std::string field;
    while (std::getline(ss, field, ',')) {
        trim(field);
        fields.push_back(std::move(field));
    }
    return fields;
}

// Parse one data row into an Order.  lineNum is used only for error messages.
Order parseRow(const std::vector<std::string>& fields, int lineNum) {
    // Expected columns: orderId, symbol, side, type, price, quantity
    if (fields.size() < 6) {
        throw std::runtime_error(
            "CSV parse error at line " + std::to_string(lineNum) +
            ": expected 6 columns, got " + std::to_string(fields.size()));
    }

    // --- orderId ---
    uint64_t orderId = 0;
    try {
        orderId = std::stoull(fields[0]);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "CSV parse error at line " + std::to_string(lineNum) +
            ": invalid orderId '" + fields[0] + "'");
    }

    // --- symbol ---
    const std::string& symbol = fields[1];
    if (symbol.empty()) {
        throw std::runtime_error(
            "CSV parse error at line " + std::to_string(lineNum) + ": empty symbol");
    }

    // --- side ---
    Side side{};
    if (iequals(fields[2], "buy")) {
        side = Side::Buy;
    } else if (iequals(fields[2], "sell")) {
        side = Side::Sell;
    } else {
        throw std::runtime_error(
            "CSV parse error at line " + std::to_string(lineNum) +
            ": unknown side '" + fields[2] + "' (expected 'buy' or 'sell')");
    }

    // --- type ---
    bool isLimit = false;
    if (iequals(fields[3], "limit")) {
        isLimit = true;
    } else if (iequals(fields[3], "market")) {
        isLimit = false;
    } else {
        throw std::runtime_error(
            "CSV parse error at line " + std::to_string(lineNum) +
            ": unknown type '" + fields[3] + "' (expected 'limit' or 'market')");
    }

    // --- price ---
    double price = 0.0;
    const bool pricePresent = !fields[4].empty();
    if (isLimit) {
        if (!pricePresent) {
            throw std::runtime_error(
                "CSV parse error at line " + std::to_string(lineNum) +
                ": limit order requires a non-empty price column");
        }
        try {
            price = std::stod(fields[4]);
        } catch (const std::exception&) {
            throw std::runtime_error(
                "CSV parse error at line " + std::to_string(lineNum) +
                ": invalid price '" + fields[4] + "'");
        }
    }

    // --- quantity ---
    uint32_t qty = 0;
    try {
        const unsigned long rawQty = std::stoul(fields[5]);
        if (rawQty == 0 || rawQty > std::numeric_limits<uint32_t>::max()) {
            throw std::range_error("out of range");
        }
        qty = static_cast<uint32_t>(rawQty);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "CSV parse error at line " + std::to_string(lineNum) +
            ": invalid quantity '" + fields[5] + "'");
    }

    // --- Construct Order ---
    if (isLimit) {
        return Order::createLimitOrder(orderId, symbol, side, price, qty);
    } else {
        return Order::createMarketOrder(orderId, symbol, side, qty);
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// CsvOrderReader
// ---------------------------------------------------------------------------

CsvOrderReader::CsvOrderReader(std::filesystem::path csvPath)
    : path_(std::move(csvPath))
{
}

std::vector<Order> CsvOrderReader::parseOrders() const {
    if (!std::filesystem::exists(path_)) {
        throw std::runtime_error("CsvOrderReader: file not found: " + path_.string());
    }

    std::ifstream file(path_);
    if (!file.is_open()) {
        throw std::runtime_error("CsvOrderReader: cannot open file: " + path_.string());
    }

    std::vector<Order> orders;
    std::string line;
    int lineNum = 0;
    bool headerSkipped = false;

    while (std::getline(file, line)) {
        ++lineNum;
        trim(line);
        if (line.empty()) continue;  // skip blank lines

        if (!headerSkipped) {
            headerSkipped = true;  // first non-blank line is always the header
            continue;
        }

        auto fields = splitCsvRow(line);
        orders.push_back(parseRow(fields, lineNum));
    }

    return orders;
}

void CsvOrderReader::replayInto(ExchangeCore& core) const {
    for (Order order : parseOrders()) {
        core.submitOrder(std::move(order));
    }
}

} // namespace miniexchange
