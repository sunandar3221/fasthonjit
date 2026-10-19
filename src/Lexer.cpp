#include "Lexer.hpp"
#include "Diagnostic.hpp"
#include <cctype>
#include <cstdlib>

Lexer::Lexer(std::string source)
    : source_(std::move(source)),
      cursor_(0),
      line_(1),
      column_(1),
      bracketNesting_(0),
      atLineStart_(true) {
    if (source_.size() >= 3 &&
        static_cast<unsigned char>(source_[0]) == 0xEF &&
        static_cast<unsigned char>(source_[1]) == 0xBB &&
        static_cast<unsigned char>(source_[2]) == 0xBF) {
        cursor_ = 3;
    }
    indentStack_.push_back(0);
    initKeywords();
}

void Lexer::initKeywords() {
    keywords_["print"] = TokenType::PRINT;
    keywords_["silent_print"] = TokenType::SILENT_PRINT;
    keywords_["if"] = TokenType::IF;
    keywords_["else"] = TokenType::ELSE;
    keywords_["elif"] = TokenType::ELIF;
    keywords_["elseif"] = TokenType::ELIF;
    keywords_["break"] = TokenType::BREAK;
    keywords_["continue"] = TokenType::CONTINUE;
    keywords_["return"] = TokenType::RETURN;
    keywords_["loop"] = TokenType::LOOP;
    keywords_["while"] = TokenType::WHILE;
    keywords_["for"] = TokenType::FOR;
    keywords_["in"] = TokenType::IN;
    keywords_["var"] = TokenType::VAR;
    keywords_["let"] = TokenType::VAR;
    keywords_["repeat"] = TokenType::REPEAT;
    keywords_["until"] = TokenType::UNTIL;
    keywords_["fn"] = TokenType::FN;
    keywords_["def"] = TokenType::FN;
    keywords_["func"] = TokenType::FN;
    keywords_["function"] = TokenType::FN;
    keywords_["use"] = TokenType::USE;
    keywords_["import"] = TokenType::USE;
    keywords_["include"] = TokenType::USE;
    keywords_["require"] = TokenType::USE;
    keywords_["new"] = TokenType::NEW;
    keywords_["get"] = TokenType::GET;
    keywords_["set"] = TokenType::SET;
    keywords_["read"] = TokenType::READ;
    keywords_["write"] = TokenType::WRITE;
    keywords_["send"] = TokenType::SEND;
    keywords_["app"] = TokenType::APP;
    keywords_["window"] = TokenType::WINDOW;
    keywords_["run"] = TokenType::RUN;
    keywords_["end"] = TokenType::END;
    keywords_["then"] = TokenType::THEN;
    keywords_["do"] = TokenType::DO;
    keywords_["and"] = TokenType::AND;
    keywords_["or"] = TokenType::OR;
    keywords_["not"] = TokenType::NOT;
    keywords_["true"] = TokenType::TRUE;
    keywords_["True"] = TokenType::TRUE;
    keywords_["TRUE"] = TokenType::TRUE;
    keywords_["false"] = TokenType::FALSE;
    keywords_["False"] = TokenType::FALSE;
    keywords_["FALSE"] = TokenType::FALSE;
    keywords_["nil"] = TokenType::NIL;
    keywords_["Nil"] = TokenType::NIL;
    keywords_["NIL"] = TokenType::NIL;
    keywords_["null"] = TokenType::NIL;
    keywords_["Null"] = TokenType::NIL;
    keywords_["NULL"] = TokenType::NIL;
    keywords_["None"] = TokenType::NIL;
    keywords_["none"] = TokenType::NIL;
    keywords_["NONE"] = TokenType::NIL;
}

bool Lexer::isAtEnd() const {
    return cursor_ >= source_.size();
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[cursor_];
}

char Lexer::peekNext() const {
    if (cursor_ + 1 >= source_.size()) return '\0';
    return source_[cursor_ + 1];
}

char Lexer::advance() {
    char c = source_[cursor_++];
    column_++;
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[cursor_] != expected) return false;
    cursor_++;
    column_++;
    return true;
}

void Lexer::handleIndentation(std::vector<Token>& /*tokens*/) {
    while (cursor_ < source_.size()) {
        char c = source_[cursor_];
        if (c == ' ') {
            cursor_++;
            column_++;
        } else if (c == '\t') {
            cursor_++;
            column_ += 4;
        } else if (c == '\r') {
            cursor_++;
        } else {
            break;
        }
    }
    atLineStart_ = false;
}

Token Lexer::readString(char quoteChar, bool isMultiline) {
    int startLine = line_;
    int startCol = column_ - (isMultiline ? 3 : 1);
    std::string val;

    while (!isAtEnd()) {
        if (isMultiline) {
            if (peek() == quoteChar && peekNext() == quoteChar && cursor_ + 2 < source_.size() && source_[cursor_ + 2] == quoteChar) {
                advance(); advance(); advance();
                return Token(TokenType::STRING, val, startLine, startCol);
            }
            if (peek() == '\n') {
                line_++;
                column_ = 0;
            }
        } else {
            // Check closing quote: ASCII quote or matching Unicode smart quotes
            if (peek() == quoteChar) {
                advance();
                return Token(TokenType::STRING, val, startLine, startCol);
            }
            if (quoteChar == '"' && static_cast<unsigned char>(peek()) == 0xE2 && cursor_ + 2 < source_.size() &&
                static_cast<unsigned char>(source_[cursor_ + 1]) == 0x80 &&
                (static_cast<unsigned char>(source_[cursor_ + 2]) == 0x9C || static_cast<unsigned char>(source_[cursor_ + 2]) == 0x9D)) {
                advance(); advance(); advance();
                return Token(TokenType::STRING, val, startLine, startCol);
            }
            if (quoteChar == '\'' && static_cast<unsigned char>(peek()) == 0xE2 && cursor_ + 2 < source_.size() &&
                static_cast<unsigned char>(source_[cursor_ + 1]) == 0x80 &&
                (static_cast<unsigned char>(source_[cursor_ + 2]) == 0x98 || static_cast<unsigned char>(source_[cursor_ + 2]) == 0x99)) {
                advance(); advance(); advance();
                return Token(TokenType::STRING, val, startLine, startCol);
            }

            // A single-line string literal must not span across newlines silently
            if (peek() == '\n' || peek() == '\r') {
                int span = std::max(1, static_cast<int>(val.size() + 1));
                std::string qStr = (quoteChar == '\'') ? "'" : "\"";
                errors_.push_back(Diagnostic::format(
                    "SyntaxError",
                    "String literal tidak ditutup (unterminated string literal).",
                    startLine, startCol, span, "",
                    "Pastikan menambahkan tanda kutip penutup '" + qStr + "' sebelum baris baru."
                ));
                return Token(TokenType::STRING, val, startLine, startCol);
            }
        }

        if (peek() == '\\') {
            advance();
            if (!isAtEnd()) {
                char esc = advance();
                if (esc == 'n') val += '\n';
                else if (esc == 't') val += '\t';
                else if (esc == 'r') val += '\r';
                else if (esc == quoteChar) val += quoteChar;
                else if (esc == '"') val += '"';
                else if (esc == '\'') val += '\'';
                else if (esc == '\\') val += '\\';
                else if (esc == '$') {
                    val += '\\';
                    val += '$';
                }
                else val += esc;
            }
        } else {
            val += advance();
        }
    }

    int span = std::max(1, static_cast<int>(val.size() + 1));
    std::string qStr = (quoteChar == '\'') ? "'" : "\"";
    errors_.push_back(Diagnostic::format(
        "SyntaxError",
        "String literal tidak ditutup (unterminated string literal).",
        startLine, startCol, span, "",
        "Pastikan menambahkan tanda kutip penutup '" + qStr + "' di akhir teks string."
    ));

    return Token(TokenType::STRING, val, startLine, startCol);
}

Token Lexer::readNumber() {
    int startLine = line_;
    int startCol = column_ - 1;
    size_t startPos = cursor_ - 1;
    bool isFloat = false;

    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        isFloat = true;
        advance();
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    std::string lexeme = source_.substr(startPos, cursor_ - startPos);
    Token tok(TokenType::NUMBER, lexeme, startLine, startCol);
    tok.isFloat = isFloat;
    if (isFloat) {
        tok.floatValue = std::strtod(lexeme.c_str(), nullptr);
    } else {
        tok.intValue = std::strtoll(lexeme.c_str(), nullptr, 10);
    }
    return tok;
}

Token Lexer::readIdentifier() {
    int startLine = line_;
    int startCol = column_ - 1;
    size_t startPos = cursor_ - 1;
    bool hasDollar = (source_[startPos] == '$');

    while (!isAtEnd()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            advance();
        } else {
            break;
        }
    }

    size_t nameStart = (hasDollar && (startPos + 1 < cursor_)) ? (startPos + 1) : startPos;
    size_t nameLen = (hasDollar && (startPos + 1 < cursor_)) ? (cursor_ - startPos - 1) : (cursor_ - startPos);
    std::string lexeme = source_.substr(nameStart, nameLen);

    auto it = keywords_.find(lexeme);
    if (it != keywords_.end() && !hasDollar) {
        return Token(it->second, lexeme, startLine, startCol);
    }
    return Token(TokenType::IDENTIFIER, lexeme, startLine, startCol);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        if (atLineStart_) {
            handleIndentation(tokens);
            if (isAtEnd()) break;
        }

        char c = advance();

        if (c == ' ' || c == '\t' || c == '\r') {
            continue;
        }

        if (c == '#') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }

        if (c == '/' && peek() == '/') {
            advance();
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }

        if (c == '/' && peek() == '*') {
            advance();
            while (!isAtEnd()) {
                if (peek() == '\n') {
                    line_++;
                    column_ = 0;
                }
                if (peek() == '*' && peekNext() == '/') {
                    advance();
                    advance();
                    break;
                }
                advance();
            }
            continue;
        }

        if (c == '-' && peek() == '-') {
            advance();
            if (peek() == '[' && peekNext() == '[') {
                advance();
                advance();
                while (!isAtEnd()) {
                    if (peek() == '\n') {
                        line_++;
                        column_ = 0;
                    }
                    if (peek() == ']' && peekNext() == ']') {
                        advance();
                        advance();
                        break;
                    }
                    advance();
                }
            } else {
                while (!isAtEnd() && peek() != '\n') {
                    advance();
                }
            }
            continue;
        }

        if (c == '\n') {
            if (bracketNesting_ == 0) {
                if (!tokens.empty() && tokens.back().type != TokenType::NEWLINE) {
                    tokens.emplace_back(TokenType::NEWLINE, "\n", line_, column_);
                }
            }
            line_++;
            column_ = 1;
            atLineStart_ = true;
            continue;
        }

        if (c == '"' || c == '\'') {
            if (peek() == c && peekNext() == c) {
                advance(); advance();
                tokens.push_back(readString(c, true));
            } else {
                tokens.push_back(readString(c, false));
            }
            continue;
        }

        // Support UTF-8 smart quotes (“ ” ‘ ’)
        if (static_cast<unsigned char>(c) == 0xE2 && cursor_ + 1 < source_.size() &&
            static_cast<unsigned char>(source_[cursor_]) == 0x80) {
            unsigned char nextByte = static_cast<unsigned char>(source_[cursor_ + 1]);
            if (nextByte == 0x9C || nextByte == 0x9D) { // “ or ”
                cursor_ += 2;
                column_ += 2;
                tokens.push_back(readString('"', false));
                continue;
            } else if (nextByte == 0x98 || nextByte == 0x99) { // ‘ or ’
                cursor_ += 2;
                column_ += 2;
                tokens.push_back(readString('\'', false));
                continue;
            }
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readNumber());
            continue;
        }

        if (c == '$') {
            if (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
                tokens.push_back(readIdentifier());
                continue;
            }
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(readIdentifier());
            continue;
        }

        int curLine = line_;
        int curCol = column_ - 1;

        switch (c) {
            case '=':
                if (match('=')) tokens.emplace_back(TokenType::EQUAL_EQUAL, "==", curLine, curCol);
                else tokens.emplace_back(TokenType::ASSIGN, "=", curLine, curCol);
                break;
            case '!':
                if (match('=')) tokens.emplace_back(TokenType::BANG_EQUAL, "!=", curLine, curCol);
                else tokens.emplace_back(TokenType::NOT, "!", curLine, curCol);
                break;
            case '~':
                if (match('=')) tokens.emplace_back(TokenType::BANG_EQUAL, "!=", curLine, curCol);
                else tokens.emplace_back(TokenType::NOT, "~", curLine, curCol);
                break;
            case '<':
                if (match('=')) tokens.emplace_back(TokenType::LESS_EQUAL, "<=", curLine, curCol);
                else tokens.emplace_back(TokenType::LESS, "<", curLine, curCol);
                break;
            case '>':
                if (match('=')) tokens.emplace_back(TokenType::GREATER_EQUAL, ">=", curLine, curCol);
                else tokens.emplace_back(TokenType::GREATER, ">", curLine, curCol);
                break;
            case '+': tokens.emplace_back(TokenType::PLUS, "+", curLine, curCol); break;
            case '-': tokens.emplace_back(TokenType::MINUS, "-", curLine, curCol); break;
            case '*': tokens.emplace_back(TokenType::STAR, "*", curLine, curCol); break;
            case '/': tokens.emplace_back(TokenType::SLASH, "/", curLine, curCol); break;
            case '%': tokens.emplace_back(TokenType::PERCENT, "%", curLine, curCol); break;
            case '(':
                bracketNesting_++;
                tokens.emplace_back(TokenType::LPAREN, "(", curLine, curCol);
                break;
            case ')':
                if (bracketNesting_ > 0) bracketNesting_--;
                tokens.emplace_back(TokenType::RPAREN, ")", curLine, curCol);
                break;
            case '[':
                bracketNesting_++;
                tokens.emplace_back(TokenType::LBRACKET, "[", curLine, curCol);
                break;
            case ']':
                if (bracketNesting_ > 0) bracketNesting_--;
                tokens.emplace_back(TokenType::RBRACKET, "]", curLine, curCol);
                break;
            case ',': tokens.emplace_back(TokenType::COMMA, ",", curLine, curCol); break;
            case ';':
                if (bracketNesting_ == 0) {
                    if (!tokens.empty() && tokens.back().type != TokenType::NEWLINE) {
                        tokens.emplace_back(TokenType::NEWLINE, ";", curLine, curCol);
                    }
                }
                break;
            case '.': tokens.emplace_back(TokenType::DOT, ".", curLine, curCol); break;
            case '&':
                if (match('&')) tokens.emplace_back(TokenType::AND, "&&", curLine, curCol);
                else tokens.emplace_back(TokenType::AND, "&", curLine, curCol);
                break;
            case '|':
                if (match('|')) tokens.emplace_back(TokenType::OR, "||", curLine, curCol);
                else tokens.emplace_back(TokenType::OR, "|", curLine, curCol);
                break;
            case ':': break;
            case '{': break;
            case '}': break;
            default:
                if (static_cast<unsigned char>(c) >= 32) {
                    errors_.push_back(Diagnostic::format("SyntaxError", "Karakter tidak terduga '" + std::string(1, c) + "'.", curLine, curCol, 1));
                }
                break;
        }
    }

    if (!tokens.empty() && tokens.back().type != TokenType::NEWLINE) {
        tokens.emplace_back(TokenType::NEWLINE, "\n", line_, column_);
    }

    tokens.emplace_back(TokenType::END_OF_FILE, "", line_, column_);
    return tokens;
}
