#[derive(Debug)]
enum Symbol {
    BnbBtc,
    BnbUsdt,
}

#[derive(Debug, Clone)]
enum OrderType {
    Buy,
    Ask,
}

#[derive(Debug, Clone)]
struct Order {
    order_type: OrderType,
    price: i64,
    quantity: i64,
}

#[derive(Debug)]
struct OrderBook {
    symbol: Symbol,
    timestamp: u64,
    first_update_id: i64,
    last_update_id: i64,
    bids: Vec<Order>,
    asks: Vec<Order>,
}

impl OrderBook {
    fn push_order(&mut self, order: &Order) {
        match order.order_type {
            OrderType::Ask => {
                for (idx, item) in self.asks.iter().enumerate() {
                    if order.price > item.price {
                        &mut self.asks.insert(idx, order.clone());
                        return;
                    }
                }
                &mut self.asks.push(order.clone());
                
            }
            OrderType::Buy => {
                for (idx, item) in self.bids.iter().enumerate() {
                    if order.price < item.price {
                        self.bids.insert(idx, order.clone());
                        return;
                    }
                }
                &mut self.bids.push(order.clone());
            }
        }
    }

    /// Prints the book content to CLI:
    /// ORDERS
    /// <price> /t /t <quantity> \t \t <price>
    fn print_book(&self)
    {

    }
}
