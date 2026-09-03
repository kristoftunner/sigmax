use sigmax::orderbook;

fn main() {
    let mut orderbook: orderbook::OrderBook = orderbook::OrderBook::new(orderbook::Symbol::BnbBtc);
    for i in 0..100 {
        if i % 2 == 0 {
            let order: orderbook::Order = orderbook::Order {
                order_type: orderbook::OrderType::Buy,
                price: 20000,
                quantity: 10000,
            };

            orderbook.push_order(&order);
        }
        let order: orderbook::Order = orderbook::Order {
            order_type: orderbook::OrderType::Ask,
            price: 100000,
            quantity: 12345678,
        };

        orderbook.push_order(&order);
    }

    let orderbook_table = orderbook.to_ascii_table();
    print!("{orderbook_table}");
}
