#include <iostream>
#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace token {
struct lbrace {
};
struct rbrace {
};
struct lbracket {
};
struct rbracket {
};
struct comma {
};
struct colon {
};
using string = std::string;
using number = double;

using type = std::variant<lbrace, rbrace, lbracket, rbracket, comma, colon,
                          string, number>;

std::ostream& operator<<(std::ostream& os, const type& tok)
{
    std::visit(overloaded{
                   [&os](lbrace) { os << "{"; },
                   [&os](rbrace) { os << "}"; },
                   [&os](lbracket) { os << "["; },
                   [&os](rbracket) { os << "]"; },
                   [&os](comma) { os << ","; },
                   [&os](colon) { os << ":"; },
                   [&os](string t) { os << '"' << t << '"'; },
                   [&os](number n) { os << n; },
                   [](auto) { throw std::runtime_error("Invalid token"); },
               },
               tok);
    return os;
}
}  // namespace token

struct value {
    std::variant<double, std::string,
                 std::unordered_map<std::string, std::shared_ptr<value>>,
                 std::vector<std::shared_ptr<value>>>
        v;
};
using value_ptr = std::shared_ptr<value>;

std::ostream& operator<<(std::ostream& os, const value& v)
{
    std::visit(
        overloaded{[&](double n) { os << n; },
                   [&](const std::string& s) { os << '"' << s << '"'; },
                   [&](const std::unordered_map<std::string, value_ptr>& m) {
                       os << "{";
                       bool first = true;
                       for (auto&& [s, v] : m) {
                           if (!first)
                               os << ", ";
                           first = false;
                           os << '"' << s << "\": " << *v;
                       }
                       os << "}";
                   },
                   [&](const std::vector<value_ptr>& v) {
                       os << "[";
                       if (v.size() != 0) {
                           os << *v[0];
                           for (size_t i = 1; i < v.size(); i++) {
                               os << ", ";
                               os << *v[i];
                           }
                       }
                       os << "]";
                   }},
        v.v);
    return os;
}

token::string tokenize_string(std::istream& is)
{
    std::string s;
    while (true) {
        int ch = is.get();
        if (ch == EOF)
            throw std::runtime_error("Invalid string");
        if (ch == '"')
            break;
        s.push_back(ch);
    }
    return s;
}

token::number tokenize_positive_number(std::istream& is, double init)
{
    double n = init;
    while (true) {
        int ch = is.peek();
        if (ch < '0' || '9' < ch)
            break;
        is.get();  // eat
        n = n * 10 + ch - '0';
    }
    return n;
}

std::vector<token::type> tokenize(std::istream& is)
{
    std::vector<token::type> ret;

    while (is) {
        int ch = is.get();
        if (ch == EOF)
            break;
        switch (ch) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            break;
        case '{':
            ret.emplace_back(token::lbrace{});
            break;
        case '}':
            ret.emplace_back(token::rbrace{});
            break;
        case '[':
            ret.emplace_back(token::lbracket{});
            break;
        case ']':
            ret.emplace_back(token::rbracket{});
            break;
        case ',':
            ret.emplace_back(token::comma{});
            break;
        case ':':
            ret.emplace_back(token::colon{});
            break;
        case '"':
            // Tokenize string
            // FIXME: escape character
            ret.emplace_back(tokenize_string(is));
            break;
        case '-':
            ret.emplace_back(-tokenize_positive_number(is, 0.0));
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ret.emplace_back(tokenize_positive_number(is, ch - '0'));
            break;
        default:
            std::cerr << ch << std::endl;
            throw std::runtime_error("Invalid letter");
        }
    }

    return ret;
}

class token_stream {
private:
    std::vector<token::type> tokens_;
    size_t head_;

private:
    token::type& cur()
    {
        if (head_ >= tokens_.size())
            throw std::runtime_error("Unexpected EOF");
        return tokens_[head_];
    }

    void eat()
    {
        head_++;
    }

public:
    token_stream(std::vector<token::type> tokens, size_t head = 0)
        : tokens_(std::move(tokens)), head_(head)
    {
    }

    template <class token_kind>
    token_kind& expect()
    {
        token_kind* t = std::get_if<token_kind>(&cur());
        if (t == nullptr)
            throw std::runtime_error("Unexpected token");
        eat();
        return *t;
    }

    template <class token_kind>
    bool match()
    {
        token_kind* t = std::get_if<token_kind>(&cur());
        return t != nullptr;
    }

    template <class token_kind>
    bool pop_if()
    {
        if (match<token_kind>()) {
            eat();
            return true;
        }
        return false;
    }

    template <class T, class... Args>
    T visit(Args&&... args)
    {
        return std::visit(
            overloaded{
                std::forward<Args>(args)...,
                [](auto) -> T { throw std::runtime_error("Invalid token"); }},
            tokens_.at(head_));
    }
};

value parse(token_stream& st);

value parse_string(token_stream& st)
{
    return value{st.expect<token::string>()};
}

value parse_number(token_stream& st)
{
    return value{st.expect<token::number>()};
}

value parse_array(token_stream& st)
{
    std::vector<value_ptr> ret;

    st.expect<token::lbracket>();
    if (st.pop_if<token::rbracket>())
        return value{ret};

    ret.push_back(std::make_shared<value>(parse(st)));
    while (!st.pop_if<token::rbracket>()) {
        st.expect<token::comma>();
        ret.push_back(std::make_shared<value>(parse(st)));
    }

    return value{ret};
}

value parse_object(token_stream& st)
{
    std::unordered_map<std::string, value_ptr> ret;

    st.expect<token::lbrace>();
    if (st.pop_if<token::rbrace>())
        return value{ret};

    auto expect_one = [&]() {
        std::string k = st.expect<token::string>();
        st.expect<token::colon>();
        value v = parse(st);
        ret.emplace(k, std::make_shared<value>(v));
    };
    expect_one();
    while (!st.pop_if<token::rbrace>()) {
        st.expect<token::comma>();
        expect_one();
    }

    return value{ret};
}

value parse(token_stream& st)
{
    return st.visit<value>([&](token::lbrace) { return parse_object(st); },
                           [&](token::lbracket) { return parse_array(st); },
                           [&](token::string) { return parse_string(st); },
                           [&](token::number) { return parse_number(st); });
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "tokenize") {
        auto tokens = tokenize(std::cin);
        for (auto&& token : tokens) {
            std::cout << token;
        }
        std::cout << std::flush;
    }
    if (argc == 2 && std::string(argv[1]) == "parse") {
        auto tokens = tokenize(std::cin);
        token_stream st{tokens};
        value v = parse(st);
        std::cout << v << std::flush;
    }

    return 0;
}
