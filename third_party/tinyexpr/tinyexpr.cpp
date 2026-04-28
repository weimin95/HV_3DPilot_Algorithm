#include "tinyexpr.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;

class Parser {
public:
    explicit Parser(const char* expression)
        : begin_(expression == nullptr ? "" : expression),
          cursor_(begin_)
    {
    }

    double parse(bool& ok)
    {
        ok = true;
        const double value = parse_logical_or(ok);
        skip_spaces();
        if (!ok || *cursor_ != '\0') {
            ok = false;
        }
        return value;
    }

    int error_position() const
    {
        return static_cast<int>(cursor_ - begin_) + 1;
    }

private:
    void skip_spaces()
    {
        while (*cursor_ == ' ' || *cursor_ == '\t' || *cursor_ == '\r' || *cursor_ == '\n') {
            ++cursor_;
        }
    }

    bool consume(const char* token)
    {
        skip_spaces();
        const size_t length = std::strlen(token);
        if (std::strncmp(cursor_, token, length) != 0) {
            return false;
        }
        cursor_ += length;
        return true;
    }

    double parse_logical_or(bool& ok)
    {
        double left = parse_logical_and(ok);
        while (ok) {
            if (consume("||")) {
                const double right = parse_logical_and(ok);
                left = (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
            }
            else {
                break;
            }
        }
        return left;
    }

    double parse_logical_and(bool& ok)
    {
        double left = parse_comparison(ok);
        while (ok) {
            if (consume("&&") || consume("&")) {
                const double right = parse_comparison(ok);
                left = (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
            }
            else {
                break;
            }
        }
        return left;
    }

    double parse_comparison(bool& ok)
    {
        double left = parse_add_sub(ok);
        while (ok) {
            if (consume("<=")) {
                const double right = parse_add_sub(ok);
                left = left <= right ? 1.0 : 0.0;
            }
            else if (consume(">=")) {
                const double right = parse_add_sub(ok);
                left = left >= right ? 1.0 : 0.0;
            }
            else if (consume("==")) {
                const double right = parse_add_sub(ok);
                left = left == right ? 1.0 : 0.0;
            }
            else if (consume("!=") || consume("<>")) {
                const double right = parse_add_sub(ok);
                left = left != right ? 1.0 : 0.0;
            }
            else if (consume("<")) {
                const double right = parse_add_sub(ok);
                left = left < right ? 1.0 : 0.0;
            }
            else if (consume(">")) {
                const double right = parse_add_sub(ok);
                left = left > right ? 1.0 : 0.0;
            }
            else {
                break;
            }
        }
        return left;
    }

    double parse_add_sub(bool& ok)
    {
        double left = parse_mul_div(ok);
        while (ok) {
            if (consume("+")) {
                left += parse_mul_div(ok);
            }
            else if (consume("-")) {
                left -= parse_mul_div(ok);
            }
            else {
                break;
            }
        }
        return left;
    }

    double parse_mul_div(bool& ok)
    {
        double left = parse_power(ok);
        while (ok) {
            if (consume("*")) {
                left *= parse_power(ok);
            }
            else if (consume("/")) {
                const double right = parse_power(ok);
                left /= right;
            }
            else if (consume("%")) {
                const double right = parse_power(ok);
                left = std::fmod(left, right);
            }
            else {
                break;
            }
        }
        return left;
    }

    double parse_power(bool& ok)
    {
        double left = parse_unary(ok);
        if (ok && consume("^")) {
            const double right = parse_power(ok);
            left = std::pow(left, right);
        }
        return left;
    }

    double parse_unary(bool& ok)
    {
        if (consume("+")) {
            return parse_unary(ok);
        }
        if (consume("-")) {
            return -parse_unary(ok);
        }
        return parse_primary(ok);
    }

    double parse_primary(bool& ok)
    {
        skip_spaces();
        if (consume("(")) {
            const double value = parse_logical_or(ok);
            if (!consume(")")) {
                ok = false;
            }
            return value;
        }

        if (is_identifier_start(*cursor_)) {
            const std::string name = parse_identifier();
            if (name == "pi") {
                return kPi;
            }
            if (name == "e") {
                return kE;
            }
            return parse_function_call(name, ok);
        }

        return parse_number(ok);
    }

    double parse_number(bool& ok)
    {
        skip_spaces();
        errno = 0;
        char* end = nullptr;
        const double value = std::strtod(cursor_, &end);
        if (end == cursor_ || errno == ERANGE) {
            ok = false;
            return 0.0;
        }
        cursor_ = end;
        return value;
    }

    static bool is_identifier_start(char ch)
    {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
    }

    static bool is_identifier_body(char ch)
    {
        return is_identifier_start(ch) || (ch >= '0' && ch <= '9');
    }

    std::string parse_identifier()
    {
        skip_spaces();
        const char* start = cursor_;
        while (is_identifier_body(*cursor_)) {
            ++cursor_;
        }
        return std::string(start, cursor_ - start);
    }

    double parse_function_call(const std::string& name, bool& ok)
    {
        if (!consume("(")) {
            ok = false;
            return 0.0;
        }

        std::vector<double> args;
        if (!consume(")")) {
            while (ok) {
                args.push_back(parse_logical_or(ok));
                if (consume(")")) {
                    break;
                }
                if (!consume(",")) {
                    ok = false;
                    break;
                }
            }
        }
        if (!ok) {
            return 0.0;
        }

        if (args.size() == 1) {
            return eval_unary(name, args[0], ok);
        }
        if (args.size() == 2) {
            return eval_binary(name, args[0], args[1], ok);
        }
        ok = false;
        return 0.0;
    }

    static double eval_unary(const std::string& name, double x, bool& ok)
    {
        if (name == "sin") return std::sin(x);
        if (name == "cos") return std::cos(x);
        if (name == "tan") return std::tan(x);
        if (name == "asin") return std::asin(x);
        if (name == "acos") return std::acos(x);
        if (name == "atan") return std::atan(x);
        if (name == "sinh") return std::sinh(x);
        if (name == "cosh") return std::cosh(x);
        if (name == "tanh") return std::tanh(x);
        if (name == "asinh") return std::asinh(x);
        if (name == "acosh") return std::acosh(x);
        if (name == "atanh") return std::atanh(x);
        if (name == "round") return std::round(x);
        if (name == "trunc") return std::trunc(x);
        if (name == "ceil") return std::ceil(x);
        if (name == "floor") return std::floor(x);
        if (name == "log") return std::log(x);
        if (name == "exp") return std::exp(x);
        if (name == "log10") return std::log10(x);
        if (name == "sqrt") return std::sqrt(x);
        if (name == "abs") return std::fabs(x);
        ok = false;
        return 0.0;
    }

    static double eval_binary(const std::string& name, double x, double y, bool& ok)
    {
        if (name == "max") return x > y ? x : y;
        if (name == "min") return x < y ? x : y;
        if (name == "pow") return std::pow(x, y);
        ok = false;
        return 0.0;
    }

private:
    const char* begin_ = nullptr;
    const char* cursor_ = nullptr;
};

}  // namespace

extern "C" double te_interp(const char* expression, int* error)
{
    Parser parser(expression);
    bool ok = false;
    const double value = parser.parse(ok);
    if (error != nullptr) {
        *error = ok ? 0 : parser.error_position();
    }
    if (!ok) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return value;
}
