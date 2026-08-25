use sigmax::orderbook;

fn main() {
    let mut orderbook: orderbook::OrderBook;
    let order: orderbook::Order = orderbook::Order {
        order_type: orderbook::OrderType::Buy,
        price: 20000,
        quantity: 10000,
    };
    orderbook.push_order(Order {});
}
