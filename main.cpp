#include <iostream>
#include <iomanip>      // setw, setprecision
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>      // ifstream, ofstream
#include <sstream>      // stringstream
#include <limits>
#include <ctime>        // time_t, localtime, strftime

#if __has_include(<filesystem>)
  #include <filesystem>
  #define HAS_STD_FILESYSTEM 1
#elif __has_include(<experimental/filesystem>)
  #include <experimental/filesystem>
  #define HAS_EXPERIMENTAL_FILESYSTEM 1
#else
  
#endif

#if defined(HAS_STD_FILESYSTEM)
  namespace fs = std::filesystem;
#elif defined(HAS_EXPERIMENTAL_FILESYSTEM)
  namespace fs = std::experimental::filesystem;
#endif

using namespace std;

/*
 * Online Food Ordering System (Console)
 * -------------------------------------
 * Files:
 *   - menu.csv   : id,name,category,price,available
 *   - orders.csv : orderId,timestamp,items(totalQty),subtotal,discount,gst,delivery,total,coupon
 *
 * Coupons:
 *   - SAVE10  : 10% off (max ₹150)
 *   - FLAT50  : Flat ₹50 off
 *
 * Billing:
 *   - GST = 5% of (subtotal - discount)
 *   - Delivery fee = ₹35 if subtotal < ₹399, else ₹0
 */

static const string MENU_FILE = "menu.csv";
static const string ORDERS_FILE = "orders.csv";

struct MenuItem {
    int id{};
    string name;
    string category;
    double price{};
    bool available{true};
};

struct CartItem {
    MenuItem item;
    int qty{};
};

class Menu {
    vector<MenuItem> items;
    int nextId = 1;

    static string trim(const string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == string::npos) return "";
        return s.substr(a, b - a + 1);
    }

public:
    const vector<MenuItem>& getAll() const { return items; }

    bool load() {
        items.clear();
        ifstream fin(MENU_FILE);
        if (!fin) return false;
        string line;
       
        bool first = true;
        while (getline(fin, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string idS, name, category, priceS, availS;
            getline(ss, idS, ',');
            getline(ss, name, ',');
            getline(ss, category, ',');
            getline(ss, priceS, ',');
            getline(ss, availS, ',');

            if (first) {
                
                bool isHeader = !all_of(idS.begin(), idS.end(), ::isdigit);
                if (isHeader) { first = false; continue; }
            }
            first = false;

            MenuItem mi;
            mi.id = stoi(trim(idS));
            mi.name = trim(name);
            mi.category = trim(category);
            mi.price = stod(trim(priceS));
            string av = trim(availS);
            mi.available = !(av == "0" || av == "false" || av == "False" || av == "FALSE");
            items.push_back(mi);
            nextId = max(nextId, mi.id + 1);
        }
        return true;
    }

    void save() const {
        ofstream fout(MENU_FILE);
        fout << "id,name,category,price,available\n";
        for (auto &mi : items) {
            fout << mi.id << ','
                 << mi.name << ','
                 << mi.category << ','
                 << fixed << setprecision(2) << mi.price << ','
                 << (mi.available ? "1" : "0") << '\n';
        }
    }

    // Seed default items on first run
    void seedIfEmpty() {
        if (load()) return; // file exists
        items = {
            {1, "Margherita Pizza", "Pizza", 249.00, true},
            {2, "Farmhouse Pizza",  "Pizza", 399.00, true},
            {3, "Masala Dosa",      "South Indian", 129.00, true},
            {4, "Paneer Butter Masala", "North Indian", 219.00, true},
            {5, "Veg Biryani",      "Rice", 199.00, true},
            {6, "Chicken Biryani",  "Rice", 249.00, true},
            {7, "Cold Coffee",      "Beverages", 99.00, true},
            {8, "Gulab Jamun",      "Desserts", 79.00, true},
        };
        nextId = 9;
        save();
    }

    vector<MenuItem> filterByCategory(const string& cat) const {
        vector<MenuItem> out;
        for (auto &mi : items)
            if (mi.available && (cat == "ALL" || cat == mi.category)) out.push_back(mi);
        return out;
    }

    vector<MenuItem> searchByName(const string& q) const {
        vector<MenuItem> out;
        string Q = q;
        transform(Q.begin(), Q.end(), Q.begin(), ::tolower);
        for (auto &mi : items) {
            string n = mi.name; transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (mi.available && n.find(Q) != string::npos) out.push_back(mi);
        }
        return out;
    }

    const MenuItem* findById(int id) const {
        for (auto &mi : items) if (mi.id == id && mi.available) return &mi;
        return nullptr;
    }

    // ADMIN
    void addItem(const string& name, const string& category, double price, bool available=true) {
        items.push_back(MenuItem{nextId++, name, category, price, available});
        save();
    }

    bool editItem(int id, const string& name, const string& category, double price, bool available) {
        for (auto &mi : items) if (mi.id == id) {
            mi.name = name; mi.category = category; mi.price = price; mi.available = available;
            save(); return true;
        }
        return false;
    }

    bool removeItem(int id) {
        auto it = remove_if(items.begin(), items.end(), [&](const MenuItem& m){ return m.id == id; });
        if (it == items.end()) return false;
        items.erase(it, items.end());
        save();
        return true;
    }
};

class Cart {
    vector<CartItem> items;

public:
    bool empty() const { return items.empty(); }

    void clear() { items.clear(); }

    void add(const MenuItem& mi, int qty) {
        if (qty <= 0) return;
        for (auto &ci : items) if (ci.item.id == mi.id) { ci.qty += qty; return; }
        items.push_back(CartItem{mi, qty});
    }

    bool updateQty(int id, int qty) {
        for (auto &ci : items) if (ci.item.id == id) {
            if (qty <= 0) { remove(id); }
            else { ci.qty = qty; }
            return true;
        }
        return false;
    }

    bool remove(int id) {
        auto it = remove_if(items.begin(), items.end(), [&](const CartItem& c){ return c.item.id == id; });
        if (it == items.end()) return false;
        items.erase(it, items.end());
        return true;
    }

    double subtotal() const {
        double s = 0;
        for (auto &ci : items) s += ci.item.price * ci.qty;
        return s;
    }

    int totalQty() const {
        int q = 0;
        for (auto &ci : items) q += ci.qty;
        return q;
    }

    const vector<CartItem>& get() const { return items; }

    void print() const {
        if (items.empty()) { cout << "Cart is empty.\n"; return; }
        cout << left << setw(5) << "ID" << setw(28) << "Item" << right << setw(6) << "Qty" << setw(10) << "Price" << setw(12) << "Line\n";
        cout << string(65, '-') << "\n";
        for (auto &ci : items) {
            double line = ci.item.price * ci.qty;
            cout << left << setw(5) << ci.item.id
                 << setw(28) << ci.item.name.substr(0,27)
                 << right << setw(6) << ci.qty
                 << setw(10) << fixed << setprecision(2) << ci.item.price
                 << setw(12) << fixed << setprecision(2) << line << "\n";
        }
        cout << string(65, '-') << "\n";
        cout << right << setw(49) << "Subtotal: " << setw(12) << fixed << setprecision(2) << subtotal() << "\n";
    }
};

struct Bill {
    double subtotal{};
    double discount{};
    double gst{};
    double delivery{};
    double total{};
    string coupon;
};

class Checkout {
public:
    static double applyCoupon(const string& code, double subtotal, double& discountOut) {
        string c = code;
        for (auto &ch : c) ch = toupper(ch);
        double discount = 0.0;
        if (c == "SAVE10") {
            discount = min(subtotal * 0.10, 150.0);
        } else if (c == "FLAT50") {
            discount = 50.0;
        }
        discountOut = discount;
        return subtotal - discount;
    }

    static Bill compute(const Cart& cart, const string& coupon) {
        Bill b;
        b.subtotal = cart.subtotal();
        double discountedSubtotal = b.subtotal;
        b.coupon = coupon;
        if (!coupon.empty())
            discountedSubtotal = applyCoupon(coupon, b.subtotal, b.discount);
        b.gst = max(0.0, discountedSubtotal * 0.05); // 5%
        b.delivery = (b.subtotal < 399.0) ? 35.0 : 0.0;
        b.total = max(0.0, discountedSubtotal + b.gst + b.delivery);
        return b;
    }

    static void printBill(const Cart& cart, const Bill& b) {
        cout << "\n===== BILL SUMMARY =====\n";
        cart.print();
        cout << fixed << setprecision(2);
        cout << right << setw(49) << "Discount: " << setw(12) << b.discount << (b.coupon.empty() ? "" : ("  (" + b.coupon + ")")) << "\n";
        cout << right << setw(49) << "GST (5%): " << setw(12) << b.gst << "\n";
        cout << right << setw(49) << "Delivery: " << setw(12) << b.delivery << "\n";
        cout << string(65, '-') << "\n";
        cout << right << setw(49) << "TOTAL: " << setw(12) << b.total << "\n";
        cout << "========================\n\n";
    }
};

class Orders {
    int nextOrderId = 1;

public:
    Orders() {
        // bootstrap next order id from file
        ifstream fin(ORDERS_FILE);
        string line; bool first = true;
        while (getline(fin, line)) {
            if (first) { first = false; continue; } // skip header if present
            if (line.empty()) continue;
            stringstream ss(line);
            string idS;
            getline(ss, idS, ',');
            if (!idS.empty() && all_of(idS.begin(), idS.end(), ::isdigit)) {
                nextOrderId = max(nextOrderId, stoi(idS) + 1);
            }
        }
    }

    int save(const Cart& cart, const Bill& b) {
        bool exists = []{
    std::ifstream test(ORDERS_FILE);
    return test.good();
}();
        ofstream fout(ORDERS_FILE, ios::app);
        if (!exists) {
            fout << "orderId,timestamp,totalQty,subtotal,discount,gst,delivery,total,coupon\n";
        }
        int id = nextOrderId++;

        // timestamp
        time_t now = time(nullptr);
        tm *lt = localtime(&now);
        char buf[64]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);

        fout << id << ','
             << buf << ','
             << cart.totalQty() << ','
             << fixed << setprecision(2) << b.subtotal << ','
             << b.discount << ','
             << b.gst << ','
             << b.delivery << ','
             << b.total << ','
             << (b.coupon.empty() ? "-" : b.coupon)
             << '\n';

        return id;
    }
};

// --------- UI helpers ----------
int readInt(const string& prompt) {
    while (true) {
        cout << prompt;
        string s; if (!getline(cin, s)) return 0;
        string t; for (char c : s) if (!isspace((unsigned char)c)) t += c;
        if (t.empty()) continue;
        try { return stoi(t); } catch (...) { cout << "Enter a valid integer.\n"; }
    }
}

double readDouble(const string& prompt) {
    while (true) {
        cout << prompt;
        string s; if (!getline(cin, s)) return 0;
        try { return stod(s); } catch (...) { cout << "Enter a valid number.\n"; }
    }
}

string readLine(const string& prompt) {
    cout << prompt;
    string s; getline(cin, s);
    return s;
}

void pauseEnter() {
    cout << "Press ENTER to continue...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

// --------- Printing helpers ----------
void printMenuTable(const vector<MenuItem>& v) {
    if (v.empty()) { cout << "No items found.\n"; return; }
    cout << left << setw(5) << "ID" << setw(28) << "Name" << setw(18) << "Category"
         << right << setw(10) << "Price" << setw(12) << "Avail\n";
    cout << string(75, '-') << "\n";
    for (auto &mi : v) {
        cout << left << setw(5) << mi.id
             << setw(28) << mi.name.substr(0,27)
             << setw(18) << mi.category.substr(0,17)
             << right << setw(10) << fixed << setprecision(2) << mi.price
             << setw(12) << (mi.available ? "Yes" : "No") << "\n";
    }
}

// --------- Admin Panel ----------
void adminPanel(Menu& menu) {
    cout << "\n== Admin Login ==\n";
    string pwd = readLine("Password: ");
    if (pwd != "admin") { cout << "Invalid password.\n"; return; }

    while (true) {
        cout << "\n--- Admin Panel ---\n";
        cout << "1) View Menu\n";
        cout << "2) Add Item\n";
        cout << "3) Edit Item\n";
        cout << "4) Remove Item\n";
        cout << "0) Back\n";
        int ch = readInt("Choose: ");
        if (ch == 0) break;
        if (ch == 1) {
            printMenuTable(menu.getAll());
        } else if (ch == 2) {
            string name = readLine("Name: ");
            string category = readLine("Category: ");
            double price = readDouble("Price (₹): ");
            string avs = readLine("Available (y/n): ");
            bool av = !(avs.size() && (avs[0]=='n' || avs[0]=='N'));
            menu.addItem(name, category, price, av);
            cout << "Item added.\n";
        } else if (ch == 3) {
            int id = readInt("Enter ID to edit: ");
            string name = readLine("New Name: ");
            string category = readLine("New Category: ");
            double price = readDouble("New Price (₹): ");
            string avs = readLine("Available (y/n): ");
            bool av = !(avs.size() && (avs[0]=='n' || avs[0]=='N'));
            if (menu.editItem(id, name, category, price, av)) cout << "Updated.\n";
            else cout << "ID not found.\n";
        } else if (ch == 4) {
            int id = readInt("Enter ID to remove: ");
            if (menu.removeItem(id)) cout << "Removed.\n";
            else cout << "ID not found.\n";
        } else {
            cout << "Invalid choice.\n";
        }
    }
}

// --------- Customer Flow ----------
void customerFlow(Menu& menu, Orders& orders) {
    Cart cart;
    string coupon;

    while (true) {
        cout << "\n=== Online Food Ordering ===\n";
        cout << "1) View All Items\n";
        cout << "2) Filter by Category\n";
        cout << "3) Search by Name\n";
        cout << "4) Sort Menu (1: Price↑ 2: Price↓ 3: Name)\n";
        cout << "5) Add to Cart\n";
        cout << "6) View/Update Cart\n";
        cout << "7) Apply Coupon\n";
        cout << "8) Checkout\n";
        cout << "0) Back\n";

        int ch = readInt("Choose: ");
        if (ch == 0) break;
        else if (ch == 1) {
            auto v = menu.getAll();
            vector<MenuItem> avail;
            for (auto &m : v) if (m.available) avail.push_back(m);
            printMenuTable(avail);
        } else if (ch == 2) {
            string cat = readLine("Category (or ALL): ");
            auto v = menu.filterByCategory(cat.empty() ? "ALL" : cat);
            printMenuTable(v);
        } else if (ch == 3) {
            string q = readLine("Search query: ");
            auto v = menu.searchByName(q);
            printMenuTable(v);
        } else if (ch == 4) {
            auto v = menu.getAll();
            vector<MenuItem> avail;
            for (auto &m : v) if (m.available) avail.push_back(m);
            int s = readInt("Sort by (1: Price↑ 2: Price↓ 3: Name): ");
            if (s == 1) sort(avail.begin(), avail.end(), [](auto&a, auto&b){ return a.price < b.price; });
            else if (s == 2) sort(avail.begin(), avail.end(), [](auto&a, auto&b){ return a.price > b.price; });
            else if (s == 3) sort(avail.begin(), avail.end(), [](auto&a, auto&b){ return a.name < b.name; });
            printMenuTable(avail);
        } else if (ch == 5) {
            int id = readInt("Enter item ID: ");
            const MenuItem* mi = menu.findById(id);
            if (!mi) { cout << "Invalid ID or unavailable.\n"; continue; }
            int qty = readInt("Quantity: ");
            if (qty <= 0) { cout << "Invalid quantity.\n"; continue; }
            cart.add(*mi, qty);
            cout << "Added to cart.\n";
        } else if (ch == 6) {
            cart.print();
            if (!cart.empty()) {
                cout << "a) Update qty  b) Remove item  c) Clear cart  d) Back\n";
                string op = readLine("Choose: ");
                if (!op.empty()) {
                    if (op[0]=='a' || op[0]=='A') {
                        int id = readInt("ID: ");
                        int q = readInt("New qty: ");
                        if (cart.updateQty(id, q)) cout << "Updated.\n"; else cout << "ID not in cart.\n";
                    } else if (op[0]=='b' || op[0]=='B') {
                        int id = readInt("ID: ");
                        if (cart.remove(id)) cout << "Removed.\n"; else cout << "ID not in cart.\n";
                    } else if (op[0]=='c' || op[0]=='C') {
                        cart.clear(); cout << "Cart cleared.\n";
                    }
                }
            }
        } else if (ch == 7) {
            coupon = readLine("Enter coupon (SAVE10 / FLAT50) or blank to remove: ");
            if (!coupon.empty()) {
                double d=0; Checkout::applyCoupon(coupon, 100.0, d); // validate format
                cout << "Coupon set to: " << coupon << "\n";
            } else {
                cout << "Coupon cleared.\n";
            }
        } else if (ch == 8) {
            if (cart.empty()) { cout << "Cart is empty.\n"; continue; }
            Bill b = Checkout::compute(cart, coupon);
            Checkout::printBill(cart, b);
            string confirm = readLine("Confirm order? (y/n): ");
            if (!confirm.empty() && (confirm[0]=='y' || confirm[0]=='Y')) {
                int orderId = orders.save(cart, b);
                cout << "Order placed! Your Order ID: #" << orderId << "\n";
                cart.clear();
                coupon.clear();
            } else {
                cout << "Checkout cancelled.\n";
            }
        } else {
            cout << "Invalid choice.\n";
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Menu menu;
    menu.seedIfEmpty(); // load or create default
    Orders orders;

    while (true) {
        cout << "\n==============================\n";
        cout << "  ONLINE FOOD ORDERING SYSTEM \n";
        cout << "==============================\n";
        cout << "1) Customer\n";
        cout << "2) Admin\n";
        cout << "0) Exit\n";
        int ch = readInt("Choose: ");
        if (ch == 0) { cout << "Goodbye!\n"; break; }
        else if (ch == 1) customerFlow(menu, orders);
        else if (ch == 2) adminPanel(menu);
        else cout << "Invalid choice.\n";
    }
    return 0;
}
