//
// Created by sudha on 14-07-2026.
//

// #ifndef MINIEXCHANGE_ORDERSTATUS_H
// #define MINIEXCHANGE_ORDERSTATUS_H
//
// #endif //MINIEXCHANGE_ORDERSTATUS_H

#pragma once

namespace miniexchange {

    enum class OrderStatus {
        New,
        PartiallyFilled,
        Filled,
        Cancelled
    };
}