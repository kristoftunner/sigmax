enum Symbol{
    BnbBtc,
    BnbUsdt
}

struct Order
{
    price: i64,
    quantity: i64,
}

struct OrderBook
{
    symbol: Symbol,
    timestamp: u64,
    first_update_id: i64,
    last_update_id: i64,
    bids: Vec<Order>,
    asks: Vec<Order>,
}
