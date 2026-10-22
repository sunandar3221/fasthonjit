#include "Value.hpp"
#include <sstream>
#include <cmath>
#include <stdexcept>

Value::Value()
    : type(ValueType::NIL), boolVal(false), intVal(0), floatVal(0.0) {}

Value::Value(bool b)
    : type(ValueType::BOOL), boolVal(b), intVal(b ? 1 : 0), floatVal(b ? 1.0 : 0.0) {}

Value::Value(int64_t i)
    : type(ValueType::INT), boolVal(i != 0), intVal(i), floatVal(static_cast<double>(i)) {}

Value::Value(int i)
    : type(ValueType::INT), boolVal(i != 0), intVal(i), floatVal(static_cast<double>(i)) {}

Value::Value(double f)
    : type(ValueType::FLOAT), boolVal(f != 0.0), intVal(static_cast<int64_t>(f)), floatVal(f) {}

Value::Value(std::string s)
    : type(ValueType::STRING), boolVal(!s.empty()), intVal(0), floatVal(0.0), strVal(std::move(s)) {}

Value::Value(const char* s)
    : type(ValueType::STRING), boolVal(s && s[0] != '\0'), intVal(0), floatVal(0.0), strVal(s ? s : "") {}

Value::Value(ValueType t, std::string s)
    : type(t), boolVal(true), intVal(0), floatVal(0.0), strVal(std::move(s)) {}

Value::Value(std::vector<Value> list)
    : type(ValueType::LIST), boolVal(true), intVal(0), floatVal(0.0), listVal(std::make_shared<std::vector<Value>>(std::move(list))) {}

Value::Value(std::unordered_map<std::string, Value> obj)
    : type(ValueType::OBJECT), boolVal(true), intVal(0), floatVal(0.0), objVal(std::make_shared<std::unordered_map<std::string, Value>>(std::move(obj))) {}

Value::Value(const Value& other)
    : type(other.type), boolVal(other.boolVal), intVal(other.intVal), floatVal(other.floatVal) {
    if (type == ValueType::STRING || type == ValueType::FUNCTION) strVal = other.strVal;
    else if (type == ValueType::LIST) listVal = other.listVal;
    else if (type == ValueType::OBJECT) objVal = other.objVal;
}

Value::Value(Value&& other) noexcept
    : type(other.type), boolVal(other.boolVal), intVal(other.intVal), floatVal(other.floatVal),
      strVal(std::move(other.strVal)), listVal(std::move(other.listVal)), objVal(std::move(other.objVal)) {}

Value& Value::operator=(const Value& other) {
    if (this == &other) return *this;
    type = other.type;
    boolVal = other.boolVal;
    intVal = other.intVal;
    floatVal = other.floatVal;
    if (type == ValueType::STRING || type == ValueType::FUNCTION) {
        strVal = other.strVal;
        listVal.reset();
        objVal.reset();
    } else if (type == ValueType::LIST) {
        listVal = other.listVal;
        strVal.clear();
        objVal.reset();
    } else if (type == ValueType::OBJECT) {
        objVal = other.objVal;
        strVal.clear();
        listVal.reset();
    } else {
        strVal.clear();
        listVal.reset();
        objVal.reset();
    }
    return *this;
}

Value& Value::operator=(Value&& other) noexcept {
    if (this == &other) return *this;
    type = other.type;
    boolVal = other.boolVal;
    intVal = other.intVal;
    floatVal = other.floatVal;
    strVal = std::move(other.strVal);
    listVal = std::move(other.listVal);
    objVal = std::move(other.objVal);
    return *this;
}

Value Value::makeList() {
    return Value(std::vector<Value>{});
}

Value Value::makeObject() {
    return Value(std::unordered_map<std::string, Value>{});
}

bool Value::isNil() const { return type == ValueType::NIL; }
bool Value::isBool() const { return type == ValueType::BOOL; }
bool Value::isInt() const { return type == ValueType::INT; }
bool Value::isFloat() const { return type == ValueType::FLOAT; }
bool Value::isNumber() const { return type == ValueType::INT || type == ValueType::FLOAT; }
bool Value::isString() const { return type == ValueType::STRING; }
bool Value::isList() const { return type == ValueType::LIST; }
bool Value::isObject() const { return type == ValueType::OBJECT; }
bool Value::isFunction() const { return type == ValueType::FUNCTION; }

bool Value::isTruthy() const {
    switch (type) {
        case ValueType::NIL: return false;
        case ValueType::BOOL: return boolVal;
        case ValueType::INT: return intVal != 0;
        case ValueType::FLOAT: return floatVal != 0.0;
        case ValueType::STRING: return !strVal.empty();
        case ValueType::LIST: return listVal && !listVal->empty();
        case ValueType::OBJECT: return objVal && !objVal->empty();
        case ValueType::FUNCTION: return !strVal.empty();
        default: return false;
    }
}

double Value::asFloat() const {
    if (type == ValueType::FLOAT) return floatVal;
    if (type == ValueType::INT) return static_cast<double>(intVal);
    if (type == ValueType::BOOL) return boolVal ? 1.0 : 0.0;
    if (type == ValueType::STRING) {
        try { return std::stod(strVal); } catch (...) { return 0.0; }
    }
    return 0.0;
}

int64_t Value::asInt() const {
    if (type == ValueType::INT) return intVal;
    if (type == ValueType::FLOAT) return static_cast<int64_t>(floatVal);
    if (type == ValueType::BOOL) return boolVal ? 1 : 0;
    if (type == ValueType::STRING) {
        try { return std::stoll(strVal); } catch (...) { return 0; }
    }
    return 0;
}

std::string Value::toString() const {
    switch (type) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return boolVal ? "true" : "false";
        case ValueType::INT: return std::to_string(intVal);
        case ValueType::FLOAT: {
            std::ostringstream ss;
            ss << floatVal;
            return ss.str();
        }
        case ValueType::STRING: return strVal;
        case ValueType::FUNCTION: return "<function " + strVal + ">";
        case ValueType::LIST: {
            if (!listVal) return "[]";
            std::string s = "[";
            for (size_t i = 0; i < listVal->size(); ++i) {
                if (i > 0) s += ", ";
                if ((*listVal)[i].isString()) {
                    s += "\"" + (*listVal)[i].toString() + "\"";
                } else {
                    s += (*listVal)[i].toString();
                }
            }
            s += "]";
            return s;
        }
        case ValueType::OBJECT: {
            if (!objVal) return "{}";
            std::string s = "{";
            bool first = true;
            for (const auto& kv : *objVal) {
                if (!first) s += ", ";
                first = false;
                s += kv.first + ": " + kv.second.toString();
            }
            s += "}";
            return s;
        }
        default: return "";
    }
}

std::string Value::getTypeName() const {
    switch (type) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return "bool";
        case ValueType::INT: return "int";
        case ValueType::FLOAT: return "float";
        case ValueType::STRING: return "str";
        case ValueType::LIST: return "list";
        case ValueType::OBJECT: return "object";
        case ValueType::FUNCTION: return "function";
        default: return "unknown";
    }
}

bool Value::operator==(const Value& other) const {
    if (type != other.type) {
        if (isNumber() && other.isNumber()) {
            return asFloat() == other.asFloat();
        }
        return false;
    }
    switch (type) {
        case ValueType::NIL: return true;
        case ValueType::BOOL: return boolVal == other.boolVal;
        case ValueType::INT: return intVal == other.intVal;
        case ValueType::FLOAT: return floatVal == other.floatVal;
        case ValueType::STRING: return strVal == other.strVal;
        case ValueType::FUNCTION: return strVal == other.strVal;
        case ValueType::LIST: {
            if (listVal == other.listVal) return true;
            if (!listVal || !other.listVal) {
                size_t sz1 = listVal ? listVal->size() : 0;
                size_t sz2 = other.listVal ? other.listVal->size() : 0;
                return sz1 == sz2;
            }
            if (listVal->size() != other.listVal->size()) return false;
            for (size_t i = 0; i < listVal->size(); ++i) {
                if ((*listVal)[i] != (*other.listVal)[i]) return false;
            }
            return true;
        }
        case ValueType::OBJECT: {
            if (objVal == other.objVal) return true;
            if (!objVal || !other.objVal) {
                size_t sz1 = objVal ? objVal->size() : 0;
                size_t sz2 = other.objVal ? other.objVal->size() : 0;
                return sz1 == sz2;
            }
            if (objVal->size() != other.objVal->size()) return false;
            for (const auto& kv : *objVal) {
                auto it = other.objVal->find(kv.first);
                if (it == other.objVal->end() || it->second != kv.second) return false;
            }
            return true;
        }
        default: return false;
    }
}

bool Value::operator!=(const Value& other) const {
    return !(*this == other);
}

Value Value::operator+(const Value& other) const {
    if (type == ValueType::INT && other.type == ValueType::INT) {
        return Value(intVal + other.intVal);
    }
    if (isString() || other.isString()) {
        return Value(toString() + other.toString());
    }
    if (isList() && other.isList()) {
        std::vector<Value> merged;
        if (listVal) merged.insert(merged.end(), listVal->begin(), listVal->end());
        if (other.listVal) merged.insert(merged.end(), other.listVal->begin(), other.listVal->end());
        return Value(merged);
    }
    return Value(asFloat() + other.asFloat());
}

Value Value::operator-(const Value& other) const {
    if (type == ValueType::INT && other.type == ValueType::INT) {
        return Value(intVal - other.intVal);
    }
    return Value(asFloat() - other.asFloat());
}

Value Value::operator*(const Value& other) const {
    if (type == ValueType::INT && other.type == ValueType::INT) {
        return Value(intVal * other.intVal);
    }
    if (isString() && other.isInt()) {
        if (other.intVal <= 0) return Value("");
        std::string res;
        res.reserve(strVal.size() * static_cast<size_t>(other.intVal));
        for (int64_t i = 0; i < other.intVal; ++i) res += strVal;
        return Value(res);
    }
    if (isInt() && other.isString()) {
        if (intVal <= 0) return Value("");
        std::string res;
        res.reserve(other.strVal.size() * static_cast<size_t>(intVal));
        for (int64_t i = 0; i < intVal; ++i) res += other.strVal;
        return Value(res);
    }
    if (isList() && other.isInt()) {
        if (other.intVal <= 0 || !listVal) return Value::makeList();
        std::vector<Value> res;
        res.reserve(listVal->size() * static_cast<size_t>(other.intVal));
        for (int64_t i = 0; i < other.intVal; ++i) {
            res.insert(res.end(), listVal->begin(), listVal->end());
        }
        return Value(res);
    }
    if (isInt() && other.isList()) {
        if (intVal <= 0 || !other.listVal) return Value::makeList();
        std::vector<Value> res;
        res.reserve(other.listVal->size() * static_cast<size_t>(intVal));
        for (int64_t i = 0; i < intVal; ++i) {
            res.insert(res.end(), other.listVal->begin(), other.listVal->end());
        }
        return Value(res);
    }
    return Value(asFloat() * other.asFloat());
}

Value Value::operator/(const Value& other) const {
    double denom = other.asFloat();
    if (denom == 0.0) {
        return Value(0.0);
    }
    if (type == ValueType::INT && other.type == ValueType::INT && (intVal % other.intVal == 0)) {
        return Value(intVal / other.intVal);
    }
    return Value(asFloat() / denom);
}

Value Value::operator%(const Value& other) const {
    int64_t denom = other.asInt();
    if (denom == 0) return Value(static_cast<int64_t>(0));
    return Value(asInt() % denom);
}

bool Value::operator<(const Value& other) const {
    if (isNil() || other.isNil()) return false;
    if (type == ValueType::INT && other.type == ValueType::INT) {
        return intVal < other.intVal;
    }
    if (isNumber() && other.isNumber()) {
        return asFloat() < other.asFloat();
    }
    if (isString() && other.isString()) {
        return strVal < other.strVal;
    }
    return false;
}

bool Value::operator>(const Value& other) const {
    if (isNil() || other.isNil()) return false;
    return other < *this;
}

bool Value::operator<=(const Value& other) const {
    if (isNil() || other.isNil()) return false;
    return (*this < other) || (*this == other);
}

bool Value::operator>=(const Value& other) const {
    if (isNil() || other.isNil()) return false;
    return (other < *this) || (*this == other);
}

Value Value::getIndex(const Value& index) const {
    if (isList()) {
        if (!listVal) return Value();
        int64_t idx = index.asInt();
        if (idx < 0) idx += static_cast<int64_t>(listVal->size());
        if (idx >= 0 && idx < static_cast<int64_t>(listVal->size())) {
            return (*listVal)[static_cast<size_t>(idx)];
        }
        return Value();
    }
    if (isObject()) {
        if (!objVal) return Value();
        auto it = objVal->find(index.toString());
        if (it != objVal->end()) return it->second;
        return Value();
    }
    if (isString()) {
        int64_t idx = index.asInt();
        if (idx < 0) idx += static_cast<int64_t>(strVal.size());
        if (idx >= 0 && idx < static_cast<int64_t>(strVal.size())) {
            return Value(std::string(1, strVal[static_cast<size_t>(idx)]));
        }
        return Value("");
    }
    return Value();
}

void Value::setIndex(const Value& index, const Value& val) {
    if (isList()) {
        if (!listVal) listVal = std::make_shared<std::vector<Value>>();
        int64_t idx = index.asInt();
        if (idx < 0) {
            idx += static_cast<int64_t>(listVal->size());
        }
        if (idx >= 0) {
            if (idx >= static_cast<int64_t>(listVal->size())) {
                listVal->resize(static_cast<size_t>(idx + 1));
            }
            (*listVal)[static_cast<size_t>(idx)] = val;
        }
    } else if (isObject()) {
        if (!objVal) objVal = std::make_shared<std::unordered_map<std::string, Value>>();
        (*objVal)[index.toString()] = val;
    }
}

Value Value::getProperty(const std::string& key) const {
    if (isObject()) {
        if (!objVal) return Value();
        auto it = objVal->find(key);
        if (it != objVal->end()) return it->second;
    }
    return Value();
}

void Value::setProperty(const std::string& key, const Value& val) {
    if (!isObject()) {
        const_cast<Value*>(this)->type = ValueType::OBJECT;
        const_cast<Value*>(this)->objVal = std::make_shared<std::unordered_map<std::string, Value>>();
    }
    if (!objVal) objVal = std::make_shared<std::unordered_map<std::string, Value>>();
    (*objVal)[key] = val;
}

std::ostream& operator<<(std::ostream& os, const Value& val) {
    os << val.toString();
    return os;
}
