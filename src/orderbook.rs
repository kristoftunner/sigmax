/// The number is represented as actual value * 1e6, a.k.a [integer digits].[6 fractional digits] in 10s
/// number space
#[derive(Debug)]
struct FixedPointNr {
    integer: i64,
    fractional: i64,
}

impl FixedPointNr {
    fn from_raw_i64(val: i64) -> FixedPointNr {
        let integer: i64 = val / 1_000_000;
        let fractional: i64 = val.clamp(0, 1_000_000);
        FixedPointNr {
            integer,
            fractional,
        }
    }
}

#[derive(Debug)]
pub enum Symbol {
    BnbBtc,
    BnbUsdt,
}

#[derive(Debug, Clone)]
pub enum OrderType {
    Buy,
    Ask,
}

#[derive(Debug, Clone)]
pub struct Order {
    pub order_type: OrderType,
    /// price, quantity are raw fixed point numbers: 8 fractional and 56 integer bits
    /// value = raw >> 8 + raw && 0xff * 10e8
    pub price: i64,
    pub quantity: i64,
}

#[derive(Debug)]
pub struct OrderBook {
    symbol: Symbol,
    /// orderbook`s symbol, stock that is tied to
    first_update_id: i64,
    /// id of the first diff-depth update in the book
    last_update_id: i64,
    /// id of the last diff-depth update in the book
    bids: Vec<Order>,
    asks: Vec<Order>,
}

impl OrderBook {
    pub fn new(symbol: Symbol) -> Self {
        Self {
            symbol: symbol,
            first_update_id: 0,
            last_update_id: 0,
            asks: Vec::new(),
            bids: Vec::new(),
        }
    }

    pub fn push_order(&mut self, order: &Order) {
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

    /// Formats the book into a printable multiline String
    /// ORDERS
    /// Bids               |Asks
    /// <price>  <quantity>|<price>  <quantity
    pub fn to_ascii_table(&self) -> String {
        // header
        let header = format!("Bids{:width$}|Asks{:width$}\n", " ", " ", width = 24)
            + format!(
                "Price{dl:10}Quantity{dl:5}|Price{dl:10}Quantity{dl:5}",
                dl = " "
            )
            .as_str();

        let max_lines = std::cmp::max(self.bids.len(), self.asks.len());
        fn format_order(order: &Order) -> String {
            let price: FixedPointNr = FixedPointNr::from_raw_i64(order.price);
            let (price_int, price_frac) = (price.integer, price.fractional);
            let qty: FixedPointNr = FixedPointNr::from_raw_i64(order.quantity);
            let (qty_int, qty_frac) = (qty.integer, qty.fractional);
            let formatted_price = format!("{price_int:7}.{price_frac:<6}")
                .split_whitespace()
                .next()
                .expect("Failed to find number")
                .to_string();
            let formatted_qty = format!("{qty_int:7}.{qty_frac:<6}")
                .split_whitespace()
                .next()
                .expect("Failed to find number")
                .to_string();
            let order = format!("{formatted_price:<13}  {formatted_qty:<13}");
            order
        }

        // content
        let mut content: String = String::from("");
        for idx in 0..max_lines {
            let mut line: String = String::from("");
            if idx < self.bids.len() {
                line += format_order(&self.bids[idx]).as_str();
            } else {
                line += " ".repeat(28).as_str();
            }
            line += "|";
            if idx < self.asks.len() {
                line += format_order(&self.asks[idx]).as_str();
            } else {
                line += " ".repeat(28).as_str();
            }
            line += "\n";
            content += line.as_str();
        }

        let full_content: String = format!("{header}\n{content}");
        full_content
    }
}
