#pragma once
#include <algorithm>
#include <chrono>
#include <cctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };

static std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static bool ieq(const std::string& a, const std::string& b) { return upper(a) == upper(b); }

static bool isUniformKeywordCase(const std::string& s) {
    bool hasLower = false, hasUpper = false;
    for (char c : s) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            hasLower = hasLower || std::islower(static_cast<unsigned char>(c));
            hasUpper = hasUpper || std::isupper(static_cast<unsigned char>(c));
        }
    }
    return !(hasLower && hasUpper);
}

static void validateKeywordCase(const std::string& token) {
    if (!isUniformKeywordCase(token)) throw Error("mixed-case keyword is not allowed: " + token);
}

static bool isCommandKeyword(const std::string& token) {
    const std::set<std::string> commands = {
        "CREATE", "DROP", "USE", "INSERT", "UPDATE", "DELETE", "SELECT", "REVERT", "SHOW", "EXIT", "QUIT"
    };
    return commands.count(upper(token)) != 0;
}

static bool isTypeKeyword(const std::string& token) {
    const std::set<std::string> types = {"INT", "STRING"};
    return types.count(upper(token)) != 0;
}

static std::string logField(std::string s) {
    for (char& c : s) {
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    }
    return s;
}

static std::string stripSqlComments(const std::string& s, bool& inBlockComment) {
    std::string out;
    bool inString = false;
    bool escaped = false;
    for (size_t i = 0; i < s.size();) {
        if (inBlockComment) {
            if (s[i] == '*' && i + 1 < s.size() && s[i + 1] == '/') {
                inBlockComment = false;
                i += 2;
            } else {
                if (s[i] == '\n') out.push_back('\n');
                ++i;
            }
            continue;
        }
        if (inString) {
            out.push_back(s[i]);
            if (escaped) escaped = false;
            else if (s[i] == '\\') escaped = true;
            else if (s[i] == '"') inString = false;
            ++i;
            continue;
        }
        if (s[i] == '"') {
            inString = true;
            out.push_back(s[i++]);
            continue;
        }
        if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') ++i;
            continue;
        }
        if (s[i] == '-' && i + 1 < s.size() && s[i + 1] == '-') {
            while (i < s.size() && s[i] != '\n') ++i;
            continue;
        }
        if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            inBlockComment = true;
            i += 2;
            continue;
        }
        out.push_back(s[i++]);
    }
    return out;
}

static size_t findCommandEnd(const std::string& s) {
    bool inString = false;
    bool escaped = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (inString) {
            if (escaped) escaped = false;
            else if (s[i] == '\\') escaped = true;
            else if (s[i] == '"') inString = false;
        } else if (s[i] == '"') {
            inString = true;
        } else if (s[i] == ';') {
            return i;
        }
    }
    return std::string::npos;
}

static std::vector<std::string> splitTop(const std::string& s, char sep) {
    std::vector<std::string> out;
    int p = 0;
    bool q = false;
    std::string cur;
    for (char c : s) {
        if (c == '"') q = !q;
        if (!q && c == '(') ++p;
        if (!q && c == ')') --p;
        if (!q && p == 0 && c == sep) {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!trim(cur).empty()) out.push_back(trim(cur));
    return out;
}

static std::string nowStamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y.%m.%d-%H:%M:%S") << "." << std::setw(3) << std::setfill('0') << ms.count();
    return os.str();
}

static bool validName(const std::string& s) {
    if (s.empty() || std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    for (char c : s) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    return true;
}

struct InternPool {
    std::set<std::string> values;
    const std::string& get(const std::string& s) { return *values.insert(s).first; }
};

struct Value {
    enum Type { Null, Int, String } type = Null;
    int i = 0;
    const std::string* s = nullptr;

    static Value null() { return {}; }
    static Value integer(int v) { Value x; x.type = Int; x.i = v; return x; }
    static Value string(const std::string& v, InternPool& pool) { Value x; x.type = String; x.s = &pool.get(v); return x; }
    std::string str() const {
        if (type == Null) return "NULL";
        if (type == Int) return std::to_string(i);
        return *s;
    }
    std::string json() const {
        if (type == Null) return "null";
        if (type == Int) return std::to_string(i);
        std::string r = "\"";
        for (char c : *s) { if (c == '"' || c == '\\') r.push_back('\\'); r.push_back(c); }
        return r + "\"";
    }
};

static int cmpValue(const Value& a, const Value& b) {
    if (a.type != b.type) return static_cast<int>(a.type) < static_cast<int>(b.type) ? -1 : 1;
    if (a.type == Value::Null) return 0;
    if (a.type == Value::Int) return (a.i < b.i) ? -1 : (a.i > b.i ? 1 : 0);
    return a.s->compare(*b.s);
}

inline bool operator<(const Value& a, const Value& b) { return cmpValue(a, b) < 0; }
inline bool operator==(const Value& a, const Value& b) { return cmpValue(a, b) == 0; }

enum class ColType { Int, String };
struct Column {
    std::string name;
    ColType type;
    bool notNull = false;
    bool indexed = false;
    bool hasDefault = false;
    Value def;
};
using Row = std::vector<Value>;

template<class K, class V, size_t ORDER = 4>
class BPlusTree {
    struct Node {
        bool leaf;
        std::vector<K> keys;
        std::vector<std::unique_ptr<Node>> child;
        std::vector<V> vals;
        Node* next = nullptr;
        explicit Node(bool l) : leaf(l) {}
    };
    std::unique_ptr<Node> root = std::make_unique<Node>(true);

    void splitChild(Node* parent, size_t idx) {
        Node* n = parent->child[idx].get();
        auto right = std::make_unique<Node>(n->leaf);
        size_t mid = n->keys.size() / 2;
        if (n->leaf) {
            right->keys.assign(n->keys.begin() + mid, n->keys.end());
            right->vals.assign(n->vals.begin() + mid, n->vals.end());
            n->keys.resize(mid);
            n->vals.resize(mid);
            right->next = n->next;
            n->next = right.get();
            parent->keys.insert(parent->keys.begin() + idx, right->keys.front());
        } else {
            K up = n->keys[mid];
            right->keys.assign(n->keys.begin() + mid + 1, n->keys.end());
            std::move(n->child.begin() + mid + 1, n->child.end(), std::back_inserter(right->child));
            n->keys.resize(mid);
            n->child.resize(mid + 1);
            parent->keys.insert(parent->keys.begin() + idx, up);
        }
        parent->child.insert(parent->child.begin() + idx + 1, std::move(right));
    }

    bool insertNonFull(Node* n, const K& k, const V& v) {
        if (n->leaf) {
            auto it = std::lower_bound(n->keys.begin(), n->keys.end(), k);
            size_t pos = static_cast<size_t>(it - n->keys.begin());
            if (it != n->keys.end() && !(*it < k) && !(k < *it)) return false;
            n->keys.insert(it, k);
            n->vals.insert(n->vals.begin() + pos, v);
            return true;
        }
        size_t i = static_cast<size_t>(std::upper_bound(n->keys.begin(), n->keys.end(), k) - n->keys.begin());
        if (n->child[i]->keys.size() >= ORDER) {
            splitChild(n, i);
            if (!(k < n->keys[i])) ++i;
        }
        return insertNonFull(n->child[i].get(), k, v);
    }

public:
    bool insert(const K& k, const V& v) {
        if (root->keys.size() >= ORDER) {
            auto nr = std::make_unique<Node>(false);
            nr->child.push_back(std::move(root));
            root = std::move(nr);
            splitChild(root.get(), 0);
        }
        return insertNonFull(root.get(), k, v);
    }
    bool find(const K& k, V& v) const {
        Node* n = root.get();
        while (!n->leaf) n = n->child[std::upper_bound(n->keys.begin(), n->keys.end(), k) - n->keys.begin()].get();
        auto it = std::lower_bound(n->keys.begin(), n->keys.end(), k);
        if (it == n->keys.end() || *it < k || k < *it) return false;
        v = n->vals[it - n->keys.begin()];
        return true;
    }
    // Serializes the current B+-tree structure to a text stream so it can be
    // persisted to disk and inspected. The dump is a level-order (BFS) listing:
    // every node gets a stable id, internal nodes list their keys and child ids,
    // leaf nodes list their key=value pairs and the id of the next leaf (the
    // linked list used for range scans). Re-emitted on every save, so the file
    // always mirrors the live index.
    void dump(std::ostream& os, const std::function<std::string(const K&)>& keyStr,
              const std::function<std::string(const V&)>& valStr) const {
        os << "ORDER " << ORDER << "\n";
        std::vector<const Node*> order;
        std::unordered_map<const Node*, size_t> id;
        std::vector<const Node*> queue{root.get()};
        for (size_t i = 0; i < queue.size(); ++i) {
            id[queue[i]] = i;
            order.push_back(queue[i]);
            if (!queue[i]->leaf)
                for (const auto& ch : queue[i]->child) queue.push_back(ch.get());
        }
        auto nodeId = [&](const Node* n) -> std::string {
            return n ? std::to_string(id[n]) : std::string("-");
        };
        for (const Node* n : order) {
            os << "node " << id[n] << (n->leaf ? " leaf" : " internal");
            if (n->leaf) {
                os << " next=" << nodeId(n->next) << " {";
                for (size_t i = 0; i < n->keys.size(); ++i) {
                    if (i) os << ", ";
                    os << keyStr(n->keys[i]) << "=" << valStr(n->vals[i]);
                }
                os << "}\n";
            } else {
                os << " keys=[";
                for (size_t i = 0; i < n->keys.size(); ++i) {
                    if (i) os << ", ";
                    os << keyStr(n->keys[i]);
                }
                os << "] children=[";
                for (size_t i = 0; i < n->child.size(); ++i) {
                    if (i) os << ", ";
                    os << id[n->child[i].get()];
                }
                os << "]\n";
            }
        }
    }

    std::vector<V> range(const K* low, bool includeLow, const K* high, bool includeHigh) const {
        std::vector<V> out;
        Node* n = root.get();
        while (!n->leaf) {
            if (low) {
                n = n->child[std::upper_bound(n->keys.begin(), n->keys.end(), *low) - n->keys.begin()].get();
            } else {
                n = n->child.front().get();
            }
        }
        while (n) {
            for (size_t i = 0; i < n->keys.size(); ++i) {
                const K& key = n->keys[i];
                if (low) {
                    bool equalLow = !(*low < key) && !(key < *low);
                    if (key < *low || (!includeLow && equalLow)) continue;
                }
                if (high) {
                    bool equalHigh = !(*high < key) && !(key < *high);
                    if (*high < key || (!includeHigh && equalHigh)) return out;
                }
                out.push_back(n->vals[i]);
            }
            n = n->next;
        }
        return out;
    }
};

class Table {
    fs::path dir;
    InternPool& pool;
public:
    std::vector<Column> cols;
    std::vector<Row> rows;
    std::unordered_map<std::string, BPlusTree<Value, size_t>> indexes;

    Table(fs::path d, InternPool& p) : dir(std::move(d)), pool(p) {}

    int colId(const std::string& name) const {
        for (size_t i = 0; i < cols.size(); ++i) if (cols[i].name == name) return static_cast<int>(i);
        throw Error("unknown column: " + name);
    }
    fs::path schemaFile() const { return dir / "schema.txt"; }
    fs::path rowsFile() const { return dir / "rows.tsv"; }
    fs::path indexFile(const std::string& col) const { return dir / ("index_" + col + ".bpt"); }

    Value parseValue(const std::string& raw, ColType type) {
        std::string x = trim(raw);
        if (ieq(x, "NULL")) { validateKeywordCase(x); return Value::null(); }
        if (type == ColType::Int) {
            try {
                size_t used = 0;
                int v = std::stoi(x, &used);
                if (used != x.size()) throw Error("expected int value: " + x);
                return Value::integer(v);
            } catch (const Error&) {
                throw;
            } catch (...) {
                throw Error("expected int value: " + x);
            }
        }
        if (x.size() < 2 || x.front() != '"' || x.back() != '"') throw Error("expected string literal: " + x);
        return Value::string(x.substr(1, x.size() - 2), pool);
    }

    void validateRow(const Row& r, int ignore = -1) const {
        for (size_t c = 0; c < cols.size(); ++c) {
            if ((cols[c].notNull || cols[c].indexed) && r[c].type == Value::Null)
                throw Error("column cannot be NULL: " + cols[c].name);
            if (r[c].type != Value::Null && ((cols[c].type == ColType::Int) != (r[c].type == Value::Int)))
                throw Error("type mismatch in column: " + cols[c].name);
            if (cols[c].indexed) {
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (static_cast<int>(i) != ignore && rows[i][c] == r[c]) throw Error("INDEXED column is not unique: " + cols[c].name);
                }
            }
        }
    }

    void rebuildIndexes() {
        indexes.clear();
        for (const auto& c : cols) if (c.indexed) indexes.emplace(c.name, BPlusTree<Value, size_t>{});
        for (size_t r = 0; r < rows.size(); ++r)
            for (size_t c = 0; c < cols.size(); ++c)
                if (cols[c].indexed) indexes[cols[c].name].insert(rows[r][c], r);
    }

    void load() {
        cols.clear(); rows.clear();
        std::ifstream s(schemaFile());
        std::string line;
        while (std::getline(s, line)) {
            if (trim(line).empty()) continue;
            auto p = splitTop(line, '|');
            Column c;
            c.name = p.at(0);
            c.type = ieq(p.at(1), "int") ? ColType::Int : ColType::String;
            c.notNull = p.at(2) == "1";
            c.indexed = p.at(3) == "1";
            c.hasDefault = p.at(4) == "1";
            if (c.hasDefault) c.def = parseValue(p.at(5), c.type);
            cols.push_back(c);
        }
        std::ifstream r(rowsFile());
        while (std::getline(r, line)) {
            auto parts = splitTop(line, '\t');
            Row row;
            for (size_t i = 0; i < cols.size(); ++i) row.push_back(parseValue(i < parts.size() ? parts[i] : "NULL", cols[i].type));
            rows.push_back(row);
        }
        rebuildIndexes();
    }

    void save() const {
        fs::create_directories(dir);
        std::ofstream s(schemaFile(), std::ios::trunc);
        for (const auto& c : cols) {
            s << c.name << '|' << (c.type == ColType::Int ? "int" : "string") << '|'
              << c.notNull << '|' << c.indexed << '|' << c.hasDefault << '|';
            if (c.hasDefault) s << (c.def.type == Value::String ? "\"" + c.def.str() + "\"" : c.def.str());
            s << '\n';
        }
        std::ofstream r(rowsFile(), std::ios::trunc);
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i) r << '\t';
                if (row[i].type == Value::String) r << '"' << row[i].str() << '"'; else r << row[i].str();
            }
            r << '\n';
        }
        saveIndexes();
    }

    // Persists every INDEXED column's B+-tree to its own index_<col>.bpt file and
    // removes index files for columns that are no longer indexed, so the on-disk
    // index always matches the current schema. Called from save() after every
    // mutation, which keeps the files updated "по ходу работы базы данных".
    void saveIndexes() const {
        std::set<std::string> live;
        for (const auto& c : cols) if (c.indexed) live.insert(c.name);
        if (fs::exists(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                std::string fn = entry.path().filename().string();
                if (fn.rfind("index_", 0) == 0 && entry.path().extension() == ".bpt") {
                    std::string col = fn.substr(6, fn.size() - 6 - 4);
                    if (!live.count(col)) fs::remove(entry.path());
                }
            }
        }
        auto valStr = [](const size_t& id) { return "row" + std::to_string(id); };
        for (const auto& c : cols) {
            if (!c.indexed) continue;
            auto it = indexes.find(c.name);
            if (it == indexes.end()) continue;
            std::ofstream f(indexFile(c.name), std::ios::trunc);
            f << "# B+-tree index for column \"" << c.name << "\" of table "
              << dir.filename().string() << "\n";
            auto keyStr = [](const Value& v) {
                return v.type == Value::String ? "\"" + v.str() + "\"" : v.str();
            };
            it->second.dump(f, keyStr, valStr);
        }
    }
};

struct Lexer {
    std::vector<std::string> t;
    size_t p = 0;
    explicit Lexer(const std::string& s) {
        for (size_t i = 0; i < s.size();) {
            if (i + 2 < s.size() && static_cast<unsigned char>(s[i]) == 0xEF && static_cast<unsigned char>(s[i + 1]) == 0xBB && static_cast<unsigned char>(s[i + 2]) == 0xBF) {
                i += 3;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(s[i]))) { ++i; continue; }
            if (s[i] == '"') {
                size_t j = i + 1;
                while (j < s.size() && s[j] != '"') { if (s[j] == '\\') ++j; ++j; }
                if (j >= s.size()) throw Error("unterminated string literal");
                t.push_back(s.substr(i, j - i + 1)); i = j + 1; continue;
            }
            if (std::ispunct(static_cast<unsigned char>(s[i])) && s[i] != '_' && s[i] != '.') {
                if (i + 1 < s.size() && (s.substr(i, 2) == "==" || s.substr(i, 2) == "!=" || s.substr(i, 2) == "<=" || s.substr(i, 2) == ">=")) {
                    t.push_back(s.substr(i, 2)); i += 2;
                } else t.push_back(std::string(1, s[i++]));
                continue;
            }
            size_t j = i;
            while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_' || s[j] == '.')) ++j;
            if (j == i) throw Error("unexpected character in command");
            t.push_back(s.substr(i, j - i)); i = j;
        }
    }
    bool has() const { return p < t.size(); }
    std::string peek() const { return has() ? t[p] : ""; }
    std::string get() { if (!has()) throw Error("unexpected end of command"); return t[p++]; }
    bool eat(const std::string& x) {
        if (has() && ieq(peek(), x)) {
            validateKeywordCase(peek());
            ++p;
            return true;
        }
        return false;
    }
    void need(const std::string& x) { if (!eat(x)) throw Error("expected: " + x); }
    void needEnd() const { if (has()) throw Error("unexpected token: " + peek()); }
};

class Telemetry {
public:
    struct Event {
        std::chrono::steady_clock::time_point at;
        double durationMs;
        bool error;
    };
private:
    std::deque<Event> events;
    // Sliding window: 10 minutes (600 seconds) of request events.
    static constexpr std::chrono::seconds windowSize() { return std::chrono::seconds(600); }

    void prune(std::chrono::steady_clock::time_point now) {
        auto cutoff = now - windowSize();
        while (!events.empty() && events.front().at < cutoff) events.pop_front();
    }
public:
    void record(std::chrono::steady_clock::time_point start,
                std::chrono::steady_clock::time_point end,
                bool error) {
        auto now = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        events.push_back({end, ms, error});
        prune(now);
    }
    std::string snapshotJson() {
        auto now = std::chrono::steady_clock::now();
        prune(now);
        int currentRps = 0;
        long long totalLastMinute = 0, errLastMinute = 0;
        double durSum = 0.0;
        int durCnt = 0;
        std::map<long long, int> perSecond;
        auto sec1 = now - std::chrono::seconds(1);
        auto sec10 = now - std::chrono::seconds(10);
        auto sec60 = now - std::chrono::seconds(60);
        for (const auto& e : events) {
            if (e.at >= sec1) ++currentRps;
            if (e.at >= sec10) { durSum += e.durationMs; ++durCnt; }
            if (e.at >= sec60) { ++totalLastMinute; if (e.error) ++errLastMinute; }
            auto s = std::chrono::duration_cast<std::chrono::seconds>(e.at.time_since_epoch()).count();
            perSecond[s]++;
        }
        double avgRps10m = 0.0;
        int maxRps10m = 0;
        if (!perSecond.empty()) {
            long long sum = 0;
            for (auto& kv : perSecond) { sum += kv.second; if (kv.second > maxRps10m) maxRps10m = kv.second; }
            avgRps10m = static_cast<double>(sum) / static_cast<double>(windowSize().count());
        }
        double avgTime10s = durCnt ? durSum / durCnt : 0.0;
        double errorRate1m = totalLastMinute ? static_cast<double>(errLastMinute) / static_cast<double>(totalLastMinute) : 0.0;
        std::ostringstream o;
        o.setf(std::ios::fixed);
        o << std::setprecision(3);
        o << "{\"current_rps\":" << currentRps
          << ",\"avg_rps_10m\":" << avgRps10m
          << ",\"max_rps_10m\":" << maxRps10m
          << ",\"avg_time_ms_10s\":" << avgTime10s
          << ",\"error_rate_1m\":" << errorRate1m
          << ",\"window_size\":" << events.size()
          << "}";
        return o.str();
    }
};

class Engine {
    fs::path root = "data";
    std::string currentDb;
    InternPool pool;
    Telemetry telemetry;
    long long requestCounter = 0;
    long long transactionCounter = 0;

    fs::path dbPath(const std::string& db) const { return root / db; }
    fs::path dbHistoryFile(const std::string& db) const { return dbPath(db) / "_db_history.log"; }
    std::pair<std::string,std::string> resolveTable(std::string name) const {
        auto dot = name.find('.');
        if (dot != std::string::npos) return {name.substr(0, dot), name.substr(dot + 1)};
        if (currentDb.empty()) throw Error("database is not selected; use USE database_name");
        return {currentDb, name};
    }
    Table openTable(const std::string& name) {
        auto [db, tb] = resolveTable(name);
        Table t(dbPath(db) / tb, pool);
        if (!fs::exists(t.schemaFile())) throw Error("table does not exist: " + name);
        t.load();
        return t;
    }
    std::vector<std::string> tableNames(const std::string& db) const {
        std::vector<std::string> names;
        fs::path base = dbPath(db);
        if (!fs::exists(base)) return names;
        for (const auto& entry : fs::directory_iterator(base)) {
            if (entry.is_directory() && fs::exists(entry.path() / "schema.txt"))
                names.push_back(entry.path().filename().string());
        }
        std::sort(names.begin(), names.end());
        return names;
    }
    std::string readWholeFile(const fs::path& p) const {
        std::ifstream in(p, std::ios::binary);
        std::ostringstream out;
        out << in.rdbuf();
        return out.str();
    }
    void writeWholeFile(const fs::path& p, const std::string& text) const {
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out << text;
    }
    void snapshotDatabase(const std::string& db) const {
        fs::path base = dbPath(db);
        if (!fs::exists(base)) return;
        std::ofstream h(dbHistoryFile(db), std::ios::app | std::ios::binary);
        h << "@" << nowStamp() << "\n";
        for (const std::string& table : tableNames(db)) {
            fs::path dir = base / table;
            h << "TABLE\t" << table << "\n";
            h << "SCHEMA\n" << readWholeFile(dir / "schema.txt") << "ENDSCHEMA\n";
            h << "ROWS\n" << readWholeFile(dir / "rows.tsv") << "ENDROWS\n";
            h << "ENDTABLE\n";
        }
        h << "#\n";
    }
    struct TableSnapshot { std::string schema; std::string rows; bool present = false; };

    std::map<std::string, TableSnapshot> parseSnapshot(const std::vector<std::string>& lines) const {
        std::map<std::string, TableSnapshot> out;
        for (size_t i = 0; i < lines.size();) {
            if (lines[i].rfind("TABLE\t", 0) != 0) throw Error("corrupted database history");
            std::string table = lines[i++].substr(6);
            if (!validName(table)) throw Error("corrupted database history");
            if (i >= lines.size() || lines[i++] != "SCHEMA") throw Error("corrupted database history");
            TableSnapshot ts; ts.present = true;
            while (i < lines.size() && lines[i] != "ENDSCHEMA") ts.schema += lines[i++] + "\n";
            if (i >= lines.size()) throw Error("corrupted database history");
            ++i;
            if (i >= lines.size() || lines[i++] != "ROWS") throw Error("corrupted database history");
            while (i < lines.size() && lines[i] != "ENDROWS") ts.rows += lines[i++] + "\n";
            if (i >= lines.size()) throw Error("corrupted database history");
            ++i;
            if (i >= lines.size() || lines[i++] != "ENDTABLE") throw Error("corrupted database history");
            out[table] = std::move(ts);
        }
        return out;
    }

    void restoreTable(const std::string& db, const std::string& table, const std::string& ts) const {
        std::ifstream h(dbHistoryFile(db), std::ios::binary);
        if (!h) throw Error("database history is empty");
        std::string line, curTs;
        std::vector<std::string> cur;
        TableSnapshot best;
        bool found = false;
        while (std::getline(h, line)) {
            if (!line.empty() && line[0] == '@') { curTs = line.substr(1); cur.clear(); }
            else if (line == "#") {
                if (curTs <= ts) {
                    auto snap = parseSnapshot(cur);
                    auto it = snap.find(table);
                    if (it != snap.end()) { best = it->second; found = true; }
                }
            }
            else cur.push_back(line);
        }
        if (!found) throw Error("no snapshot for table before timestamp: " + table);
        fs::path dir = dbPath(db) / table;
        fs::create_directories(dir);
        writeWholeFile(dir / "schema.txt", best.schema);
        writeWholeFile(dir / "rows.tsv", best.rows);
    }
    void log(long long reqId, long long txnId,
             const std::string& q, const std::string& start, const std::string& end,
             const std::string& status, int code) {
        fs::create_directories(root);
        std::ofstream f(root / "access.log", std::ios::app);
        f << "date=" << start
          << " request_id=" << reqId
          << " transaction_id=" << txnId
          << " start=" << start
          << " end=" << end
          << " client=local handler=cli"
          << " code=" << code
          << " status=" << logField(status)
          << " query=" << logField(q) << "\n";
    }
    ColType parseColumnType(Lexer& lx) {
        std::string typ = lx.get();
        if (isTypeKeyword(typ)) validateKeywordCase(typ);
        if (ieq(typ, "int")) return ColType::Int;
        if (ieq(typ, "string")) return ColType::String;
        throw Error("unknown type: " + typ);
    }
    std::string rest(Lexer& lx) {
        std::string out;
        while (lx.has()) out += (out.empty() ? "" : " ") + lx.get();
        if (trim(out).empty()) throw Error("expected condition");
        return out;
    }
    bool tryParseColumnValue(Table& t, int c, const std::string& tok, Value& v) {
        try {
            v = t.parseValue(tok, t.cols[c].type);
            return true;
        } catch (...) {
            return false;
        }
    }
    Value literalOrColumn(Table& t, const Row& r, const std::string& tok) {
        if (!tok.empty() && tok.front() == '"') return Value::string(tok.substr(1, tok.size() - 2), pool);
        if (ieq(tok, "NULL")) { validateKeywordCase(tok); return Value::null(); }
        if (!tok.empty() && (std::isdigit(static_cast<unsigned char>(tok[0])) || tok[0] == '-')) {
            try {
                size_t used = 0;
                int v = std::stoi(tok, &used);
                if (used != tok.size()) throw Error("expected int value: " + tok);
                return Value::integer(v);
            } catch (const Error&) {
                throw;
            } catch (...) {
                throw Error("expected int value: " + tok);
            }
        }
        return r[t.colId(tok)];
    }
    bool predicate(Table& t, const Row& r, Lexer& lx) {
        if (lx.eat("(")) { bool v = expr(t, r, lx); lx.need(")"); return v; }
        std::string left = lx.get();
        if (lx.eat("BETWEEN")) {
            Value a = literalOrColumn(t, r, left), lo = literalOrColumn(t, r, lx.get());
            lx.need("AND");
            Value hi = literalOrColumn(t, r, lx.get());
            return cmpValue(a, lo) >= 0 && cmpValue(a, hi) < 0;
        }
        if (lx.eat("LIKE")) {
            Value a = literalOrColumn(t, r, left), pat = literalOrColumn(t, r, lx.get());
            if (pat.type != Value::String) throw Error("LIKE pattern must be a string");
            if (a.type == Value::Null) return false;
            if (a.type != Value::String) throw Error("LIKE requires string value");
            try {
                return std::regex_match(*a.s, std::regex(*pat.s));
            } catch (const std::regex_error& e) {
                throw Error(std::string("invalid LIKE regex: ") + e.what());
            }
        }
        std::string op = lx.get();
        Value a = literalOrColumn(t, r, left), b = literalOrColumn(t, r, lx.get());
        int c = cmpValue(a, b);
        if (op == "==") return c == 0; if (op == "!=") return c != 0; if (op == "<") return c < 0;
        if (op == ">") return c > 0; if (op == "<=") return c <= 0; if (op == ">=") return c >= 0;
        throw Error("unknown comparison operator: " + op);
    }
    bool andExpr(Table& t, const Row& r, Lexer& lx) {
        bool v = predicate(t, r, lx);
        while (lx.eat("AND")) { bool rhs = predicate(t, r, lx); v = v && rhs; }
        return v;
    }
    bool expr(Table& t, const Row& r, Lexer& lx) {
        bool v = andExpr(t, r, lx);
        while (lx.eat("OR")) { bool rhs = andExpr(t, r, lx); v = v || rhs; }
        return v;
    }
    std::vector<size_t> filter(Table& t, const std::string& where) {
        std::vector<size_t> ids;
        if (trim(where).empty()) { for (size_t i = 0; i < t.rows.size(); ++i) ids.push_back(i); return ids; }
        Lexer quick(where);
        if (quick.t.size() == 3 && t.indexes.count(quick.t[0])) {
            int c = t.colId(quick.t[0]);
            Value k;
            if (tryParseColumnValue(t, c, quick.t[2], k)) {
                const std::string& op = quick.t[1];
                if (op == "==") {
                    size_t id = 0;
                    if (t.indexes[quick.t[0]].find(k, id)) ids.push_back(id);
                    return ids;
                }
                if (op == "<") ids = t.indexes[quick.t[0]].range(nullptr, false, &k, false);
                else if (op == "<=") ids = t.indexes[quick.t[0]].range(nullptr, false, &k, true);
                else if (op == ">") ids = t.indexes[quick.t[0]].range(&k, false, nullptr, false);
                else if (op == ">=") ids = t.indexes[quick.t[0]].range(&k, true, nullptr, false);
                else ids.clear();
                if (op == "<" || op == "<=" || op == ">" || op == ">=") {
                    std::sort(ids.begin(), ids.end());
                    return ids;
                }
            }
        }
        if (quick.t.size() == 5 && ieq(quick.t[1], "BETWEEN") && ieq(quick.t[3], "AND") && t.indexes.count(quick.t[0])) {
            int c = t.colId(quick.t[0]);
            Value lo, hi;
            if (tryParseColumnValue(t, c, quick.t[2], lo) && tryParseColumnValue(t, c, quick.t[4], hi)) {
                ids = t.indexes[quick.t[0]].range(&lo, true, &hi, false);
                std::sort(ids.begin(), ids.end());
                return ids;
            }
        }
        if (t.rows.empty()) {
            Row dummy;
            for (const auto& col : t.cols)
                dummy.push_back(col.type == ColType::Int ? Value::integer(0) : Value::string("", pool));
            Lexer lx(where);
            expr(t, dummy, lx);
            lx.needEnd();
            return ids;
        }
        for (size_t i = 0; i < t.rows.size(); ++i) {
            Lexer lx(where);
            if (expr(t, t.rows[i], lx)) ids.push_back(i);
            lx.needEnd();
        }
        return ids;
    }
    std::string jsonRows(Table& t, const std::vector<size_t>& ids, const std::vector<std::pair<std::string,std::string>>& cols) {
        std::ostringstream o; o << "[";
        for (size_t k = 0; k < ids.size(); ++k) {
            if (k) o << ",";
            o << "{";
            for (size_t i = 0; i < cols.size(); ++i) {
                if (i) o << ",";
                int c = t.colId(cols[i].first);
                o << "\"" << cols[i].second << "\":" << t.rows[ids[k]][c].json();
            }
            o << "}";
        }
        o << "]";
        return o.str();
    }

public:
    bool sessionEnded = false;

    std::string exec(const std::string& q0) {
        std::string q = trim(q0);
        if (!q.empty() && q.back() == ';') q.pop_back();
        std::string startStamp = nowStamp();
        auto startTp = std::chrono::steady_clock::now();
        ++requestCounter;
        ++transactionCounter;
        long long reqId = requestCounter;
        long long txnId = transactionCounter;
        try {
            std::string r = run(q);
            auto endTp = std::chrono::steady_clock::now();
            telemetry.record(startTp, endTp, false);
            log(reqId, txnId, q, startStamp, nowStamp(), "OK", 0);
            return r;
        } catch (const std::exception& e) {
            auto endTp = std::chrono::steady_clock::now();
            telemetry.record(startTp, endTp, true);
            log(reqId, txnId, q, startStamp, nowStamp(), std::string("ERROR:") + e.what(), 1);
            return std::string("ERROR: ") + e.what();
        }
    }

    std::string run(const std::string& q) {
        Lexer lx(q);
        if (!lx.has()) return "";
        std::string rawCmd = lx.get();
        if (isCommandKeyword(rawCmd)) validateKeywordCase(rawCmd);
        std::string cmd = upper(rawCmd);
        if (cmd == "CREATE" && lx.eat("DATABASE")) {
            std::string db = lx.get(); if (!validName(db)) throw Error("invalid database name");
            lx.needEnd();
            fs::create_directories(dbPath(db)); snapshotDatabase(db); return "OK";
        }
        if (cmd == "DROP" && lx.eat("DATABASE")) {
            std::string db = lx.get(); if (!validName(db)) throw Error("invalid database name");
            lx.needEnd();
            fs::remove_all(dbPath(db)); if (currentDb == db) currentDb.clear(); return "OK";
        }
        if (cmd == "USE") {
            std::string db = lx.get(); if (!fs::exists(dbPath(db))) throw Error("database does not exist: " + db);
            lx.needEnd();
            currentDb = db; return "OK";
        }
        if (cmd == "CREATE" && lx.eat("TABLE")) {
            std::string name = lx.get();
            std::string db, tb;
            auto dot = name.find('.');
            if (dot != std::string::npos) {
                db = name.substr(0, dot);
                tb = name.substr(dot + 1);
            } else {
                if (currentDb.empty()) throw Error("database is not selected");
                db = currentDb;
                tb = name;
            }
            if (!validName(db)) throw Error("invalid database name");
            if (!validName(tb)) throw Error("invalid table name");
            if (!fs::exists(dbPath(db))) throw Error("database does not exist: " + db);
            Table t(dbPath(db) / tb, pool);
            if (fs::exists(t.schemaFile())) throw Error("table already exists: " + name);
            lx.need("(");
            while (!lx.eat(")")) {
                Column c; c.name = lx.get(); if (!validName(c.name)) throw Error("invalid column name");
                for (const auto& existing : t.cols)
                    if (existing.name == c.name) throw Error("duplicate column: " + c.name);
                c.type = parseColumnType(lx);
                while (!lx.eat(",") && lx.peek() != ")") {
                    if (lx.eat("NOT_NULL")) c.notNull = true;
                    else if (lx.eat("INDEXED")) { c.indexed = true; c.notNull = true; }
                    else if (lx.eat("DEFAULT")) { c.hasDefault = true; c.def = t.parseValue(lx.get(), c.type); }
                    else throw Error("unknown column modifier: " + lx.peek());
                }
                t.cols.push_back(c);
            }
            lx.needEnd();
            t.save(); snapshotDatabase(db); return "OK";
        }
        if (cmd == "DROP" && lx.eat("TABLE")) {
            auto [db, tb] = resolveTable(lx.get());
            lx.needEnd();
            fs::remove_all(dbPath(db) / tb); snapshotDatabase(db); return "OK";
        }
        if (cmd == "INSERT") {
            lx.need("INTO"); std::string name = lx.get(); Table t = openTable(name);
            lx.need("(");
            std::vector<std::string> names;
            while (true) {
                std::string col = lx.get();
                t.colId(col);
                if (std::find(names.begin(), names.end(), col) != names.end()) throw Error("duplicate column in INSERT: " + col);
                names.push_back(col);
                if (lx.eat(")")) break;
                lx.need(",");
            }
            lx.need("VALUE");
            do {
                lx.need("("); Row row(t.cols.size(), Value::null());
                for (size_t i = 0; i < t.cols.size(); ++i) if (t.cols[i].hasDefault) row[i] = t.cols[i].def;
                for (size_t i = 0; i < names.size(); ++i) {
                    int c = t.colId(names[i]); row[c] = t.parseValue(lx.get(), t.cols[c].type);
                    if (i + 1 < names.size()) lx.need(",");
                }
                lx.need(")");
                t.validateRow(row); t.rows.push_back(row);
            } while (lx.eat(","));
            lx.needEnd();
            auto [db, tb] = resolveTable(name);
            t.rebuildIndexes(); t.save(); snapshotDatabase(db); return "OK";
        }
        if (cmd == "UPDATE") {
            std::string name = lx.get(); Table t = openTable(name);
            lx.need("SET"); std::vector<std::pair<int,Value>> set;
            while (true) {
                std::string c = lx.get(); lx.need("="); int id = t.colId(c); set.push_back({id, t.parseValue(lx.get(), t.cols[id].type)});
                if (lx.eat("WHERE")) break;
                lx.need(",");
            }
            std::string where = rest(lx);
            for (size_t id : filter(t, where)) { Row nr = t.rows[id]; for (auto& p : set) nr[p.first] = p.second; t.validateRow(nr, static_cast<int>(id)); t.rows[id] = nr; }
            auto [db, tb] = resolveTable(name);
            t.rebuildIndexes(); t.save(); snapshotDatabase(db); return "OK";
        }
        if (cmd == "DELETE") {
            lx.need("FROM"); std::string name = lx.get(); Table t = openTable(name);
            lx.need("WHERE"); std::string where = rest(lx);
            auto ids = filter(t, where); std::set<size_t> del(ids.begin(), ids.end()); std::vector<Row> keep;
            for (size_t i = 0; i < t.rows.size(); ++i) if (!del.count(i)) keep.push_back(t.rows[i]);
            auto [db, tb] = resolveTable(name);
            t.rows = keep; t.rebuildIndexes(); t.save(); snapshotDatabase(db); return "OK";
        }
        if (cmd == "SELECT") {
            std::vector<std::pair<std::string,std::string>> out;
            bool aggregate = false;
            struct Agg { std::string fn, col, alias; }; std::vector<Agg> aggs;
            if (lx.eat("*")) {}
            else {
                lx.need("(");
                while (true) {
                    std::string first = lx.get();
                    if (lx.eat("(")) {
                        if (ieq(first, "COUNT") || ieq(first, "SUM") || ieq(first, "AVG")) validateKeywordCase(first);
                        std::string col = lx.get(); lx.need(")");
                        std::string alias = upper(first) + "_" + col; if (lx.eat("AS")) alias = lx.get();
                        aggs.push_back({upper(first), col, alias}); aggregate = true;
                    } else {
                        std::string alias = first; if (lx.eat("AS")) alias = lx.get();
                        out.push_back({first, alias});
                    }
                    if (lx.eat(")")) break;
                    lx.need(",");
                }
            }
            lx.need("FROM"); std::string name = lx.get(); Table t = openTable(name);
            if (out.empty() && !aggregate) for (auto& c : t.cols) out.push_back({c.name, c.name});
            if (aggregate && !out.empty()) throw Error("cannot mix aggregate and regular columns");
            std::string where; if (lx.eat("WHERE")) where = rest(lx); else lx.needEnd();
            auto ids = filter(t, where);
            if (!aggregate) return jsonRows(t, ids, out);
            std::ostringstream o; o << "[{";
            for (size_t a = 0; a < aggs.size(); ++a) {
                if (a) o << ",";
                int c = t.colId(aggs[a].col); long long sum = 0, count = 0;
                for (size_t id : ids) if (t.rows[id][c].type == Value::Int) { sum += t.rows[id][c].i; ++count; }
                o << "\"" << aggs[a].alias << "\":";
                if (aggs[a].fn == "COUNT") o << ids.size();
                else if (aggs[a].fn == "SUM") o << sum;
                else if (aggs[a].fn == "AVG") o << (count ? static_cast<double>(sum) / count : 0);
                else throw Error("unknown aggregate: " + aggs[a].fn);
            }
            return o.str() + "}]";
        }
        if (cmd == "REVERT") {
            std::string name = lx.get(), ts;
            while (lx.has()) ts += lx.get();
            if (!std::regex_match(ts, std::regex(R"(\d{4}\.\d{2}\.\d{2}-\d{2}:\d{2}:\d{2}\.\d{3})")))
                throw Error("invalid timestamp format");
            auto [db, tb] = resolveTable(name);
            if (!fs::exists(dbPath(db))) throw Error("database does not exist: " + db);
            restoreTable(db, tb, ts);
            // restoreTable rewrites schema/rows directly, so reload the table and
            // re-save it to regenerate its B+-tree index files from the data.
            if (fs::exists(dbPath(db) / tb / "schema.txt")) {
                Table t(dbPath(db) / tb, pool);
                t.load();
                t.save();
            }
            snapshotDatabase(db); return "OK";
        }
        if (cmd == "SHOW") {
            std::string what = lx.get();
            if (ieq(what, "TELEMETRY")) {
                validateKeywordCase(what);
                lx.needEnd();
                return telemetry.snapshotJson();
            }
            throw Error("unknown SHOW target: " + what);
        }
        if (cmd == "EXIT" || cmd == "QUIT") {
            lx.needEnd();
            sessionEnded = true;
            return "BYE";
        }
        throw Error("unsupported command");
    }
};

static std::vector<std::string> readCommands(std::istream& in) {
    std::vector<std::string> cmds; std::string line, cur;
    bool inBlockComment = false;
    while (std::getline(in, line)) {
        cur += stripSqlComments(line + "\n", inBlockComment);
        size_t p;
        while ((p = findCommandEnd(cur)) != std::string::npos) {
            cmds.push_back(cur.substr(0, p + 1));
            cur.erase(0, p + 1);
        }
    }
    if (inBlockComment) throw Error("unterminated block comment");
    if (!trim(cur).empty()) throw Error("last command is not terminated by ';'");
    return cmds;
}

