#pragma once

#include <windows.h>
#include <shlobj.h>
#include <iphlpapi.h>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

#ifndef DROPEFFECT_COPY
#define DROPEFFECT_COPY 1
#endif
#ifndef DROPEFFECT_MOVE
#define DROPEFFECT_MOVE 2
#endif

namespace tools {

// ---------------------------------------------------------------------------
// 1. Clipboard Copy & Cut
// ---------------------------------------------------------------------------
inline bool CopyTextToClipboard(const std::wstring& text) {
    if (!OpenClipboard(nullptr)) {
        return false;
    }
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* pMem = GlobalLock(hMem);
        if (pMem) {
            memcpy(pMem, text.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
    return true;
}

inline bool CopyOrCutFileToClipboard(const std::wstring& filePath, bool isCut) {
    if (filePath.empty()) return false;
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    // 1. CF_HDROP (Shell file list)
    size_t pathLen = filePath.size();
    size_t dropFilesSize = sizeof(DROPFILES);
    size_t totalBytes = dropFilesSize + (pathLen + 2) * sizeof(wchar_t);

    HGLOBAL hDrop = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalBytes);
    if (hDrop) {
        char* pData = static_cast<char*>(GlobalLock(hDrop));
        if (pData) {
            DROPFILES* pDrop = reinterpret_cast<DROPFILES*>(pData);
            pDrop->pFiles = static_cast<DWORD>(dropFilesSize);
            pDrop->fWide = TRUE;
            wchar_t* pDest = reinterpret_cast<wchar_t*>(pData + dropFilesSize);
            memcpy(pDest, filePath.c_str(), pathLen * sizeof(wchar_t));
            pDest[pathLen] = L'\0';
            pDest[pathLen + 1] = L'\0';
            GlobalUnlock(hDrop);
            SetClipboardData(CF_HDROP, hDrop);
        } else {
            GlobalFree(hDrop);
        }
    }

    // 2. Preferred DropEffect (DROPEFFECT_COPY or DROPEFFECT_MOVE)
    UINT uDropEffect = RegisterClipboardFormatW(L"Preferred DropEffect");
    if (uDropEffect) {
        HGLOBAL hEffect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
        if (hEffect) {
            DWORD* pEffect = static_cast<DWORD*>(GlobalLock(hEffect));
            if (pEffect) {
                *pEffect = isCut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
                GlobalUnlock(hEffect);
                SetClipboardData(uDropEffect, hEffect);
            } else {
                GlobalFree(hEffect);
            }
        }
    }

    // 3. CF_UNICODETEXT (fallback for text fields & text editors)
    size_t textBytes = (pathLen + 1) * sizeof(wchar_t);
    HGLOBAL hText = GlobalAlloc(GMEM_MOVEABLE, textBytes);
    if (hText) {
        wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hText));
        if (pText) {
            memcpy(pText, filePath.c_str(), textBytes);
            GlobalUnlock(hText);
            SetClipboardData(CF_UNICODETEXT, hText);
        } else {
            GlobalFree(hText);
        }
    }

    CloseClipboard();
    return true;
}

// ---------------------------------------------------------------------------
// 2. Local IPv4 Resolver & Network Interfaces
// ---------------------------------------------------------------------------
struct LocalIPv4Info {
    std::wstring ip;
    std::wstring adapterType;
};

inline std::vector<LocalIPv4Info> GetLocalIPv4Addresses() {
    std::vector<LocalIPv4Info> list;
    ULONG size = 0;
    if (GetIpAddrTable(nullptr, &size, FALSE) == ERROR_INSUFFICIENT_BUFFER && size > 0) {
        std::vector<BYTE> buf(size);
        PMIB_IPADDRTABLE pTable = reinterpret_cast<PMIB_IPADDRTABLE>(buf.data());
        if (GetIpAddrTable(pTable, &size, FALSE) == NO_ERROR) {
            for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
                DWORD addr = pTable->table[i].dwAddr;
                BYTE b1 = static_cast<BYTE>((addr >> 0) & 0xFF);
                BYTE b2 = static_cast<BYTE>((addr >> 8) & 0xFF);
                BYTE b3 = static_cast<BYTE>((addr >> 16) & 0xFF);
                BYTE b4 = static_cast<BYTE>((addr >> 24) & 0xFF);

                // Filter loopback (127.x), zero (0.0.0.0), APIPA (169.254.x)
                if (b1 == 127 || b1 == 0 || (b1 == 169 && b2 == 254)) {
                    continue;
                }

                wchar_t ipBuf[64];
                swprintf_s(ipBuf, L"%u.%u.%u.%u", b1, b2, b3, b4);

                std::wstring type = L"Local IPv4";
                if (b1 == 192 && b2 == 168) {
                    type = L"Wi-Fi / LAN (192.168.x.x)";
                } else if (b1 == 10) {
                    type = L"Private Network (10.x.x.x)";
                } else if (b1 == 172 && (b2 >= 16 && b2 <= 31)) {
                    type = L"Private Network (172.16.x.x)";
                } else if (b1 == 26) {
                    type = L"Virtual Network / VPN";
                }

                list.push_back({ipBuf, type});
            }
        }
    }
    return list;
}

struct NetworkInterfaceInfo {
    std::wstring ip;
    std::wstring mask;
    std::wstring gateway;
    std::wstring adapterName;
    std::wstring typeLabel;
    std::wstring glyph;
};

inline std::vector<NetworkInterfaceInfo> GetNetworkInterfaces() {
    std::vector<NetworkInterfaceInfo> list;
    ULONG size = 0;
    DWORD ret = GetAdaptersInfo(nullptr, &size);
    if (ret == ERROR_BUFFER_OVERFLOW && size > 0) {
        std::vector<BYTE> buf(size);
        PIP_ADAPTER_INFO pInfo = reinterpret_cast<PIP_ADAPTER_INFO>(buf.data());
        if (GetAdaptersInfo(pInfo, &size) == NO_ERROR) {
            for (PIP_ADAPTER_INFO pCurr = pInfo; pCurr != nullptr; pCurr = pCurr->Next) {
                std::string desc = pCurr->Description;
                std::string descLower = desc;
                for (char& c : descLower) c = static_cast<char>(tolower(c));

                std::wstring typeLabel = L"Network";
                std::wstring glyph = L"\uE701";

                if (pCurr->Type == 71 || descLower.find("wi-fi") != std::string::npos || descLower.find("wireless") != std::string::npos) {
                    typeLabel = L"Wi-Fi";
                    glyph = L"\uE704";
                } else if (descLower.find("vpn") != std::string::npos || descLower.find("radmin") != std::string::npos || descLower.find("wireguard") != std::string::npos || descLower.find("tailscale") != std::string::npos || descLower.find("tap") != std::string::npos) {
                    typeLabel = L"VPN";
                    glyph = L"\uE701";
                } else if (pCurr->Type == 6) {
                    typeLabel = L"Ethernet";
                    glyph = L"\uE839";
                }

                int wlen = MultiByteToWideChar(CP_ACP, 0, desc.c_str(), -1, nullptr, 0);
                std::wstring wDesc(wlen > 1 ? wlen - 1 : 0, L'\0');
                if (wlen > 1) {
                    MultiByteToWideChar(CP_ACP, 0, desc.c_str(), -1, &wDesc[0], wlen);
                }

                std::string gw = (pCurr->GatewayList.IpAddress.String[0] != '\0' && strcmp(pCurr->GatewayList.IpAddress.String, "0.0.0.0") != 0) ? pCurr->GatewayList.IpAddress.String : "";
                std::wstring wGw(gw.begin(), gw.end());

                for (IP_ADDR_STRING* pIp = &(pCurr->IpAddressList); pIp != nullptr; pIp = pIp->Next) {
                    std::string ipStr = pIp->IpAddress.String;
                    if (ipStr.empty() || ipStr == "0.0.0.0" || ipStr.starts_with("127.")) {
                        continue;
                    }
                    std::string maskStr = pIp->IpMask.String;
                    std::wstring wIp(ipStr.begin(), ipStr.end());
                    std::wstring wMask(maskStr.begin(), maskStr.end());

                    list.push_back({
                        wIp,
                        wMask,
                        wGw,
                        wDesc,
                        typeLabel,
                        glyph
                    });
                }
            }
        }
    }

    if (list.empty()) {
        auto fallback = GetLocalIPv4Addresses();
        for (const auto& fb : fallback) {
            list.push_back({
                fb.ip,
                L"",
                L"",
                fb.adapterType,
                L"Local IPv4",
                L"\uE701"
            });
        }
    }
    return list;
}

// ---------------------------------------------------------------------------
// 3. String & Number Formatting Helpers
// ---------------------------------------------------------------------------
inline std::wstring FormatCleanNumber(double val) {
    if (std::isnan(val) || std::isinf(val)) {
        return L"Error";
    }
    // Check if effectively integer
    double rounded = std::round(val);
    if (std::abs(val - rounded) < 1e-9 && std::abs(val) < 1e15) {
        return std::to_wstring(static_cast<long long>(rounded));
    }
    // Decimal formatting
    wchar_t buf[64];
    swprintf_s(buf, L"%.6f", val);
    std::wstring s = buf;
    while (!s.empty() && s.back() == L'0') s.pop_back();
    if (!s.empty() && s.back() == L'.') s.pop_back();
    return s;
}

inline std::wstring ToLower(const std::wstring& str) {
    std::wstring res = str;
    for (auto& c : res) c = static_cast<wchar_t>(towlower(c));
    return res;
}

inline std::wstring Trim(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return L"";
    size_t last = str.find_last_not_of(L" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// ---------------------------------------------------------------------------
// 4. Math Expression Evaluator
// ---------------------------------------------------------------------------
namespace detail {

struct Token {
    enum Type { Number, Plus, Minus, Mul, Div, Mod, Pow, LParen, RParen, End, Ident } type;
    double value = 0.0;
    std::wstring text;
};

inline bool IsIdentChar(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c == L'_');
}

inline std::vector<Token> Tokenize(const std::wstring& expr) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t n = expr.size();

    while (i < n) {
        wchar_t c = expr[i];
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') {
            i++;
            continue;
        }

        // Check for Hex prefix: 0x or 0X
        if (c == L'0' && i + 1 < n && (expr[i + 1] == L'x' || expr[i + 1] == L'X')) {
            size_t start = i + 2;
            size_t end = start;
            while (end < n && iswxdigit(expr[end])) end++;
            if (end > start) {
                std::wstring hexStr = expr.substr(start, end - start);
                wchar_t* pEnd = nullptr;
                unsigned long long val = wcstoull(hexStr.c_str(), &pEnd, 16);
                tokens.push_back({Token::Number, static_cast<double>(val), L""});
                i = end;
                continue;
            }
        }

        // Numbers: digits or '.' followed by digit
        if (iswdigit(c) || (c == L'.' && i + 1 < n && iswdigit(expr[i + 1]))) {
            size_t start = i;
            while (i < n && (iswdigit(expr[i]) || expr[i] == L'.')) i++;
            // Scientific notation: e+10, e-5
            if (i < n && (expr[i] == L'e' || expr[i] == L'E')) {
                i++;
                if (i < n && (expr[i] == L'+' || expr[i] == L'-')) i++;
                while (i < n && iswdigit(expr[i])) i++;
            }
            std::wstring numStr = expr.substr(start, i - start);
            wchar_t* pEnd = nullptr;
            double val = wcstod(numStr.c_str(), &pEnd);
            tokens.push_back({Token::Number, val, L""});
            continue;
        }

        // Operators & Parens
        if (c == L'+') { tokens.push_back({Token::Plus}); i++; continue; }
        if (c == L'-') { tokens.push_back({Token::Minus}); i++; continue; }
        if (c == L'*') { tokens.push_back({Token::Mul}); i++; continue; }
        if (c == L'x' || c == L'X') {
            // Can be multiply if preceded by a number or closing paren, and followed by whitespace or number
            if (!tokens.empty() && (tokens.back().type == Token::Number || tokens.back().type == Token::RParen)) {
                tokens.push_back({Token::Mul});
                i++;
                continue;
            }
        }
        if (c == L'/') { tokens.push_back({Token::Div}); i++; continue; }
        if (c == L'%') { tokens.push_back({Token::Mod}); i++; continue; }
        if (c == L'^') { tokens.push_back({Token::Pow}); i++; continue; }
        if (c == L'(') { tokens.push_back({Token::LParen}); i++; continue; }
        if (c == L')') { tokens.push_back({Token::RParen}); i++; continue; }

        // Identifiers / function names / constants
        if (IsIdentChar(c)) {
            size_t start = i;
            while (i < n && IsIdentChar(expr[i])) i++;
            std::wstring word = ToLower(expr.substr(start, i - start));
            if (word == L"pi") {
                tokens.push_back({Token::Number, 3.14159265358979323846, L""});
            } else if (word == L"e") {
                tokens.push_back({Token::Number, 2.71828182845904523536, L""});
            } else {
                tokens.push_back({Token::Ident, 0.0, word});
            }
            continue;
        }

        // Unrecognized character -> cannot parse
        return {};
    }

    tokens.push_back({Token::End});
    return tokens;
}

class Parser {
   public:
    explicit Parser(std::vector<Token> t) : tokens(std::move(t)), pos(0) {}

    bool Parse(double& result) {
        if (tokens.empty() || tokens.front().type == Token::End) return false;
        try {
            result = ParseExpression();
            return Current().type == Token::End && !std::isnan(result) && !std::isinf(result);
        } catch (...) {
            return false;
        }
    }

   private:
    const std::vector<Token> tokens;
    size_t pos;

    const Token& Current() const {
        return (pos < tokens.size()) ? tokens[pos] : tokens.back();
    }

    void Consume() {
        if (pos < tokens.size()) pos++;
    }

    // Expression = Term (('+' | '-') Term)*
    double ParseExpression() {
        double left = ParseTerm();
        while (true) {
            if (Current().type == Token::Plus) {
                Consume();
                left += ParseTerm();
            } else if (Current().type == Token::Minus) {
                Consume();
                left -= ParseTerm();
            } else {
                break;
            }
        }
        return left;
    }

    // Term = Power (('*' | '/' | '%') Power)*
    double ParseTerm() {
        double left = ParsePower();
        while (true) {
            if (Current().type == Token::Mul) {
                Consume();
                left *= ParsePower();
            } else if (Current().type == Token::Div) {
                Consume();
                double right = ParsePower();
                if (std::abs(right) < 1e-15) throw false;
                left /= right;
            } else if (Current().type == Token::Mod) {
                Consume();
                double right = ParsePower();
                if (std::abs(right) < 1e-15) throw false;
                left = std::fmod(left, right);
            } else {
                break;
            }
        }
        return left;
    }

    // Power = Factor ('^' Factor)*
    double ParsePower() {
        double left = ParseFactor();
        if (Current().type == Token::Pow) {
            Consume();
            double right = ParsePower(); // right associative
            left = std::pow(left, right);
        }
        return left;
    }

    // Factor = ('+' | '-')? Primary
    double ParseFactor() {
        if (Current().type == Token::Plus) {
            Consume();
            return ParseFactor();
        }
        if (Current().type == Token::Minus) {
            Consume();
            return -ParseFactor();
        }
        return ParsePrimary();
    }

    // Primary = Number | '(' Expression ')' | Ident '(' Expression ')'
    double ParsePrimary() {
        const Token& cur = Current();
        if (cur.type == Token::Number) {
            double v = cur.value;
            Consume();
            return v;
        }
        if (cur.type == Token::LParen) {
            Consume();
            double v = ParseExpression();
            if (Current().type != Token::RParen) throw false;
            Consume();
            return v;
        }
        if (cur.type == Token::Ident) {
            std::wstring fn = cur.text;
            Consume();
            if (Current().type != Token::LParen) throw false;
            Consume();
            double arg = ParseExpression();
            if (Current().type != Token::RParen) throw false;
            Consume();

            if (fn == L"sqrt") {
                if (arg < 0.0) throw false;
                return std::sqrt(arg);
            } else if (fn == L"cbrt") {
                return std::cbrt(arg);
            } else if (fn == L"abs") {
                return std::abs(arg);
            } else if (fn == L"sin") {
                return std::sin(arg);
            } else if (fn == L"cos") {
                return std::cos(arg);
            } else if (fn == L"tan") {
                return std::tan(arg);
            } else if (fn == L"log" || fn == L"log10") {
                if (arg <= 0.0) throw false;
                return std::log10(arg);
            } else if (fn == L"ln") {
                if (arg <= 0.0) throw false;
                return std::log(arg);
            } else if (fn == L"round") {
                return std::round(arg);
            } else if (fn == L"floor") {
                return std::floor(arg);
            } else if (fn == L"ceil") {
                return std::ceil(arg);
            }
            throw false;
        }
        throw false;
    }
};

} // namespace detail

inline bool IsStandaloneNumber(const std::wstring& input, double& outNum) {
    std::wstring t = Trim(input);
    if (t.empty()) return false;

    wchar_t* pEnd = nullptr;
    double val = wcstod(t.c_str(), &pEnd);
    if (pEnd && *pEnd == L'\0' && pEnd != t.c_str()) {
        outNum = val;
        return true;
    }
    return false;
}

inline bool EvaluateMath(const std::wstring& input, double& outResult) {
    std::wstring trimmed = Trim(input);
    if (trimmed.empty()) return false;

    // Handle "X% of Y" pattern
    std::wstring lower = ToLower(trimmed);
    size_t ofPos = lower.find(L"% of ");
    if (ofPos != std::wstring::npos) {
        std::wstring part1 = Trim(trimmed.substr(0, ofPos));
        std::wstring part2 = Trim(trimmed.substr(ofPos + 5));
        double pct = 0, base = 0;
        auto evalVal = [](const std::wstring& s, double& val) -> bool {
            if (IsStandaloneNumber(s, val)) return true;
            return EvaluateMath(s, val);
        };
        if (evalVal(part1, pct) && evalVal(part2, base)) {
            outResult = (pct / 100.0) * base;
            return true;
        }
    }

    auto tokens = detail::Tokenize(trimmed);
    if (tokens.size() <= 1) return false;

    // Ensure it contains at least one operator or function or paren, not just a standalone number
    bool hasOperator = false;
    for (const auto& t : tokens) {
        if (t.type != detail::Token::Number && t.type != detail::Token::End) {
            hasOperator = true;
            break;
        }
    }
    if (!hasOperator) return false;

    detail::Parser parser(std::move(tokens));
    return parser.Parse(outResult);
}

inline bool EvaluateConversionFormula(const std::wstring& formula, double n, double& outResult) {
    std::wstring expr = Trim(formula);
    if (expr.empty()) return false;

    // Check if formula is just a numeric multiplier (e.g. "0.621371")
    wchar_t* pEnd = nullptr;
    double mult = wcstod(expr.c_str(), &pEnd);
    if (pEnd && *pEnd == L'\0' && pEnd != expr.c_str()) {
        outResult = n * mult;
        return true;
    }

    std::wstring nStr = FormatCleanNumber(n);
    if (n < 0) nStr = L"(" + nStr + L")";

    auto replaceAll = [](std::wstring& s, const std::wstring& from, const std::wstring& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::wstring::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(expr, L"{n}", nStr);
    replaceAll(expr, L"{N}", nStr);
    replaceAll(expr, L"{x}", nStr);
    replaceAll(expr, L"{X}", nStr);

    for (size_t i = 0; i < expr.size(); ++i) {
        if (expr[i] == L'x' || expr[i] == L'X') {
            bool leftOk = (i == 0 || (!iswalnum(expr[i - 1]) && expr[i - 1] != L'_'));
            bool rightOk = (i + 1 == expr.size() || (!iswalnum(expr[i + 1]) && expr[i + 1] != L'_'));
            if (leftOk && rightOk) {
                expr.replace(i, 1, nStr);
                i += nStr.size() - 1;
            }
        }
    }

    return EvaluateMath(expr, outResult);
}

// ---------------------------------------------------------------------------
// 5. Standalone Number & Multi-Unit Conversions (/c)
// ---------------------------------------------------------------------------
struct UnitConversionResult {
    std::wstring title;
    std::wstring subtitle;
    std::wstring copyText;
};

inline bool ParseConversionQuery(const std::wstring& input, double& outNum, std::wstring& outUnit, bool& isHelp) {
    std::wstring t = Trim(input);
    if (t.starts_with(L"/c") || t.starts_with(L"/C")) {
        t = Trim(t.substr(2));
    } else {
        return false;
    }

    if (t.empty()) {
        isHelp = true;
        return true;
    }
    isHelp = false;

    size_t i = 0;
    if (i < t.size() && (t[i] == L'+' || t[i] == L'-')) i++;
    bool hasDigits = false;
    bool hasDot = false;
    while (i < t.size()) {
        if (iswdigit(t[i])) {
            hasDigits = true;
            i++;
        } else if (t[i] == L'.' && !hasDot) {
            hasDot = true;
            i++;
        } else {
            break;
        }
    }

    if (!hasDigits) return false;

    std::wstring numPart = t.substr(0, i);
    wchar_t* pEnd = nullptr;
    outNum = wcstod(numPart.c_str(), &pEnd);

    std::wstring rest = Trim(t.substr(i));
    outUnit = ToLower(rest);
    return true;
}

inline std::wstring NormalizeUnit(const std::wstring& unitRaw) {
    std::wstring u = ToLower(Trim(unitRaw));
    if (u == L"c" || u == L"\u00B0c" || u == L"celsius") return L"c";
    if (u == L"f" || u == L"\u00B0f" || u == L"fahrenheit") return L"f";
    if (u == L"k" || u == L"kelvin") return L"k";
    if (u == L"km" || u == L"kilometer" || u == L"kilometers") return L"km";
    if (u == L"mi" || u == L"mile" || u == L"miles") return L"miles";
    if (u == L"m" || u == L"meter" || u == L"meters") return L"m";
    if (u == L"cm" || u == L"centimeter" || u == L"centimeters") return L"cm";
    if (u == L"mm" || u == L"millimeter" || u == L"millimeters") return L"mm";
    if (u == L"in" || u == L"inch" || u == L"inches" || u == L"\"") return L"in";
    if (u == L"ft" || u == L"foot" || u == L"feet" || u == L"'") return L"feet";
    if (u == L"yd" || u == L"yard" || u == L"yards") return L"yd";
    if (u == L"kg" || u == L"kilo" || u == L"kilogram" || u == L"kilograms") return L"kg";
    if (u == L"lb" || u == L"lbs" || u == L"pound" || u == L"pounds") return L"lbs";
    if (u == L"g" || u == L"gram" || u == L"grams") return L"g";
    if (u == L"oz" || u == L"ounce" || u == L"ounces") return L"oz";
    if (u == L"kmh" || u == L"km/h" || u == L"kph") return L"km/h";
    if (u == L"mph") return L"mph";
    if (u == L"b" || u == L"bytes" || u == L"byte") return L"bytes";
    if (u == L"kb" || u == L"kilobyte" || u == L"kilobytes") return L"kb";
    if (u == L"mb" || u == L"megabyte" || u == L"megabytes") return L"mb";
    if (u == L"gb" || u == L"gigabyte" || u == L"gigabytes") return L"gb";
    if (u == L"tb" || u == L"terabyte" || u == L"terabytes") return L"tb";
    if (u == L"s" || u == L"sec" || u == L"second" || u == L"seconds") return L"s";
    if (u == L"min" || u == L"minute" || u == L"minutes") return L"min";
    if (u == L"h" || u == L"hr" || u == L"hrs" || u == L"hour" || u == L"hours") return L"hours";
    if (u == L"d" || u == L"day" || u == L"days") return L"days";
    if (u == L"l" || u == L"liter" || u == L"liters") return L"l";
    if (u == L"gal" || u == L"gallon" || u == L"gallons") return L"gal";
    if (u == L"bar") return L"bar";
    if (u == L"psi") return L"psi";
    if (u == L"kw") return L"kw";
    if (u == L"hp") return L"hp";
    return u;
}

inline std::vector<UnitConversionResult> GenerateUnitConversions(double n, const std::wstring& reqUnitRaw) {
    std::vector<UnitConversionResult> list;
    std::wstring numStr = FormatCleanNumber(n);
    std::wstring u = ToLower(Trim(reqUnitRaw));

    auto add = [&](const std::wstring& fromUnit, double toVal, const std::wstring& toUnit, const std::wstring& cat) {
        std::wstring toStr = FormatCleanNumber(toVal);
        list.push_back({
            numStr + L" " + fromUnit + L" = " + toStr + L" " + toUnit,
            cat + L" \u2022 Press Enter to copy " + toStr + L" " + toUnit,
            toStr + L" " + toUnit
        });
    };

    // 1. Specific Unit requested
    if (!u.empty()) {
        if (u == L"c" || u == L"\u00B0c" || u == L"celsius") {
            add(L"\u00B0C", (n * 9.0 / 5.0) + 32.0, L"\u00B0F", L"Temperature");
            add(L"\u00B0C", n + 273.15, L"K", L"Temperature");
        } else if (u == L"f" || u == L"\u00B0f" || u == L"fahrenheit") {
            add(L"\u00B0F", (n - 32.0) * 5.0 / 9.0, L"\u00B0C", L"Temperature");
            add(L"\u00B0F", (n - 32.0) * 5.0 / 9.0 + 273.15, L"K", L"Temperature");
        } else if (u == L"k" || u == L"kelvin") {
            add(L"K", n - 273.15, L"\u00B0C", L"Temperature");
            add(L"K", (n - 273.15) * 9.0 / 5.0 + 32.0, L"\u00B0F", L"Temperature");
        } else if (u == L"km" || u == L"kilometer" || u == L"kilometers") {
            add(L"km", n * 0.621371, L"miles", L"Distance");
            add(L"km", n * 1000.0, L"meters", L"Distance");
            add(L"km", n * 3280.84, L"feet", L"Distance");
            add(L"km", n * 1093.61, L"yards", L"Distance");
        } else if (u == L"mi" || u == L"mile" || u == L"miles") {
            add(L"miles", n * 1.60934, L"km", L"Distance");
            add(L"miles", n * 5280.0, L"feet", L"Distance");
            add(L"miles", n * 1760.0, L"yards", L"Distance");
        } else if (u == L"m" || u == L"meter" || u == L"meters") {
            add(L"m", n * 3.28084, L"feet", L"Length");
            add(L"m", n * 1.09361, L"yards", L"Length");
            add(L"m", n * 39.3701, L"inches", L"Length");
            add(L"m", n / 1000.0, L"km", L"Distance");
        } else if (u == L"cm" || u == L"centimeter" || u == L"centimeters") {
            add(L"cm", n / 2.54, L"inches", L"Length");
            add(L"cm", n / 30.48, L"feet", L"Length");
            add(L"cm", n * 10.0, L"mm", L"Length");
            add(L"cm", n / 100.0, L"m", L"Length");
        } else if (u == L"mm" || u == L"millimeter" || u == L"millimeters") {
            add(L"mm", n / 25.4, L"inches", L"Length");
            add(L"mm", n / 10.0, L"cm", L"Length");
        } else if (u == L"in" || u == L"inch" || u == L"inches" || u == L"\"") {
            add(L"in", n * 2.54, L"cm", L"Length");
            add(L"in", n * 25.4, L"mm", L"Length");
            add(L"in", n / 12.0, L"feet", L"Length");
        } else if (u == L"ft" || u == L"foot" || u == L"feet" || u == L"'") {
            add(L"ft", n * 0.3048, L"m", L"Length");
            add(L"ft", n * 30.48, L"cm", L"Length");
            add(L"ft", n * 12.0, L"inches", L"Length");
            add(L"ft", n / 3.0, L"yards", L"Length");
        } else if (u == L"yd" || u == L"yard" || u == L"yards") {
            add(L"yd", n * 0.9144, L"m", L"Length");
            add(L"yd", n * 3.0, L"feet", L"Length");
        } else if (u == L"kg" || u == L"kilo" || u == L"kilogram" || u == L"kilograms") {
            add(L"kg", n * 2.20462, L"lbs", L"Weight");
            add(L"kg", n * 1000.0, L"grams", L"Weight");
            add(L"kg", n * 35.274, L"oz", L"Weight");
            add(L"kg", n / 1000.0, L"tonnes", L"Weight");
        } else if (u == L"lb" || u == L"lbs" || u == L"pound" || u == L"pounds") {
            add(L"lbs", n / 2.20462, L"kg", L"Weight");
            add(L"lbs", n * 16.0, L"oz", L"Weight");
            add(L"lbs", n * 453.592, L"grams", L"Weight");
        } else if (u == L"g" || u == L"gram" || u == L"grams") {
            add(L"g", n * 0.035274, L"oz", L"Weight");
            add(L"g", n / 453.592, L"lbs", L"Weight");
            add(L"g", n / 1000.0, L"kg", L"Weight");
        } else if (u == L"oz" || u == L"ounce" || u == L"ounces") {
            add(L"oz", n * 28.3495, L"grams", L"Weight");
            add(L"oz", n / 16.0, L"lbs", L"Weight");
        } else if (u == L"kmh" || u == L"km/h" || u == L"kph") {
            add(L"km/h", n * 0.621371, L"mph", L"Speed");
            add(L"km/h", n / 3.6, L"m/s", L"Speed");
            add(L"km/h", n * 0.539957, L"knots", L"Speed");
        } else if (u == L"mph") {
            add(L"mph", n * 1.60934, L"km/h", L"Speed");
            add(L"mph", n * 0.44704, L"m/s", L"Speed");
            add(L"mph", n * 0.868976, L"knots", L"Speed");
        } else if (u == L"b" || u == L"bytes" || u == L"byte") {
            add(L"Bytes", n / 1024.0, L"KB", L"Digital Storage");
            add(L"Bytes", n / (1024.0 * 1024.0), L"MB", L"Digital Storage");
            add(L"Bytes", n * 8.0, L"Bits", L"Digital Storage");
        } else if (u == L"kb" || u == L"kilobyte" || u == L"kilobytes") {
            add(L"KB", n / 1024.0, L"MB", L"Digital Storage");
            add(L"KB", n * 1024.0, L"Bytes", L"Digital Storage");
            add(L"KB", n / (1024.0 * 1024.0), L"GB", L"Digital Storage");
        } else if (u == L"mb" || u == L"megabyte" || u == L"megabytes") {
            add(L"MB", n / 1024.0, L"GB", L"Digital Storage");
            add(L"MB", n * 1024.0, L"KB", L"Digital Storage");
            add(L"MB", n / (1024.0 * 1024.0), L"TB", L"Digital Storage");
        } else if (u == L"gb" || u == L"gigabyte" || u == L"gigabytes") {
            add(L"GB", n / 1024.0, L"TB", L"Digital Storage");
            add(L"GB", n * 1024.0, L"MB", L"Digital Storage");
            add(L"GB", n * 1024.0 * 1024.0, L"KB", L"Digital Storage");
        } else if (u == L"tb" || u == L"terabyte" || u == L"terabytes") {
            add(L"TB", n * 1024.0, L"GB", L"Digital Storage");
            add(L"TB", n / 1024.0, L"PB", L"Digital Storage");
        } else if (u == L"s" || u == L"sec" || u == L"second" || u == L"seconds") {
            add(L"s", n / 60.0, L"minutes", L"Time");
            add(L"s", n / 3600.0, L"hours", L"Time");
            add(L"s", n * 1000.0, L"ms", L"Time");
        } else if (u == L"min" || u == L"minute" || u == L"minutes") {
            add(L"min", n / 60.0, L"hours", L"Time");
            add(L"min", n * 60.0, L"seconds", L"Time");
            add(L"min", n / 1440.0, L"days", L"Time");
        } else if (u == L"h" || u == L"hr" || u == L"hrs" || u == L"hour" || u == L"hours") {
            add(L"hours", n / 24.0, L"days", L"Time");
            add(L"hours", n * 60.0, L"minutes", L"Time");
            add(L"hours", n * 3600.0, L"seconds", L"Time");
        } else if (u == L"d" || u == L"day" || u == L"days") {
            add(L"days", n / 7.0, L"weeks", L"Time");
            add(L"days", n * 24.0, L"hours", L"Time");
            add(L"days", n / 365.25, L"years", L"Time");
        } else if (u == L"l" || u == L"liter" || u == L"liters") {
            add(L"L", n * 0.264172, L"US gal", L"Volume");
            add(L"L", n * 1000.0, L"ml", L"Volume");
            add(L"L", n * 33.814, L"fl oz", L"Volume");
            add(L"L", n * 4.22675, L"cups", L"Volume");
        } else if (u == L"gal" || u == L"gallon" || u == L"gallons") {
            add(L"US gal", n * 3.78541, L"L", L"Volume");
            add(L"US gal", n * 128.0, L"fl oz", L"Volume");
        } else if (u == L"bar") {
            add(L"bar", n * 14.5038, L"psi", L"Pressure");
            add(L"bar", n * 100.0, L"kPa", L"Pressure");
            add(L"bar", n * 0.986923, L"atm", L"Pressure");
        } else if (u == L"psi") {
            add(L"psi", n * 0.0689476, L"bar", L"Pressure");
            add(L"psi", n * 6.89476, L"kPa", L"Pressure");
        } else if (u == L"kw") {
            add(L"kW", n * 1.34102, L"hp", L"Power");
            add(L"kW", n * 1000.0, L"Watts", L"Power");
        } else if (u == L"hp") {
            add(L"hp", n * 0.7457, L"kW", L"Power");
            add(L"hp", n * 745.7, L"Watts", L"Power");
        }
    }

    // 2. If no specific unit was requested or matched, generate ALL standard conversion pairs
    if (list.empty()) {
        // Temperature
        add(L"\u00B0C", (n * 9.0 / 5.0) + 32.0, L"\u00B0F", L"Temperature");
        add(L"\u00B0F", (n - 32.0) * 5.0 / 9.0, L"\u00B0C", L"Temperature");
        add(L"\u00B0C", n + 273.15, L"K", L"Temperature");

        // Distance & Length
        add(L"km", n * 0.621371, L"miles", L"Distance");
        add(L"miles", n * 1.60934, L"km", L"Distance");
        add(L"m", n * 3.28084, L"feet", L"Length");
        add(L"feet", n * 0.3048, L"m", L"Length");
        add(L"cm", n / 2.54, L"inches", L"Length");
        add(L"inches", n * 2.54, L"cm", L"Length");
        add(L"mm", n / 25.4, L"inches", L"Length");
        add(L"yd", n * 0.9144, L"m", L"Length");

        // Weight / Mass
        add(L"kg", n * 2.20462, L"lbs", L"Weight");
        add(L"lbs", n / 2.20462, L"kg", L"Weight");
        add(L"g", n * 0.035274, L"oz", L"Weight");
        add(L"oz", n * 28.3495, L"g", L"Weight");

        // Speed
        add(L"km/h", n * 0.621371, L"mph", L"Speed");
        add(L"mph", n * 1.60934, L"km/h", L"Speed");
        add(L"m/s", n * 3.6, L"km/h", L"Speed");

        // Digital Storage
        if (n >= 1.0) {
            add(L"MB", n / 1024.0, L"GB", L"Digital Storage");
            add(L"GB", n * 1024.0, L"MB", L"Digital Storage");
            add(L"GB", n / 1024.0, L"TB", L"Digital Storage");
            add(L"TB", n * 1024.0, L"GB", L"Digital Storage");
            add(L"KB", n / 1024.0, L"MB", L"Digital Storage");
        }

        // Time
        if (n >= 1.0) {
            add(L"hours", n / 24.0, L"days", L"Time");
            add(L"days", n / 7.0, L"weeks", L"Time");
            add(L"minutes", n / 60.0, L"hours", L"Time");
            add(L"seconds", n / 60.0, L"minutes", L"Time");
        }

        // Volume
        add(L"L", n * 0.264172, L"US gal", L"Volume");
        add(L"US gal", n * 3.78541, L"L", L"Volume");
        add(L"ml", n * 0.033814, L"fl oz", L"Volume");

        // Pressure
        add(L"bar", n * 14.5038, L"psi", L"Pressure");
        add(L"psi", n * 0.0689476, L"bar", L"Pressure");

        // Power
        add(L"kW", n * 1.34102, L"hp", L"Power");
        add(L"hp", n * 0.7457, L"kW", L"Power");

        // Radix
        if (n >= 0.0 && n <= 16777215.0 && (n == std::floor(n))) {
            unsigned long long intVal = static_cast<unsigned long long>(n);
            wchar_t hexBuf[32];
            swprintf_s(hexBuf, L"0x%llX", intVal);
            std::wstring binStr = L"0b";
            if (intVal == 0) {
                binStr += L"0";
            } else {
                for (int b = 31; b >= 0; --b) {
                    if ((intVal >> b) & 1) {
                        for (int j = b; j >= 0; --j) {
                            binStr.push_back(((intVal >> j) & 1) ? L'1' : L'0');
                        }
                        break;
                    }
                }
            }
            wchar_t octBuf[32];
            swprintf_s(octBuf, L"0o%llo", intVal);

            list.push_back({
                numStr + L" = " + hexBuf + L" (Hex) = " + binStr + L" (Bin) = " + octBuf + L" (Oct)",
                L"Base Radix \u2022 Press Enter to copy " + std::wstring(hexBuf),
                hexBuf
            });
        }
    }

    return list;
}

inline std::vector<UnitConversionResult> GenerateCommonConversions(double n) {
    return GenerateUnitConversions(n, L"");
}

} // namespace tools
