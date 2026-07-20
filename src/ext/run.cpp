#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include "command_registry.h"
namespace fs = std::filesystem;
namespace {
const size_t MIN_MATCH = 6;
const size_t STRONG_MATCH = 8;
const size_t MIN_NIST_ROWS = 300;
const char* INDEX_FORMAT = "FORMAT 4";
struct Constant {
    std::string name, unit;
    std::string digits;
    long ls_exp = 0;
    bool exact = false;
    std::string certain;
    long offset = 0;
};
struct ConsSeq {
    std::string anum, name, digits;
    long offset = 0;
    bool has_kw = false;
    bool has_name = false;
    bool malformed = false;
};
std::string home_dir() {
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    return home ? std::string(home) : ".";
}
std::string rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
std::string lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return r;
}
std::string strip_leading_zeros(const std::string& s) {
    size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0') ++i;
    return s.substr(i);
}
std::vector<std::string> split_columns(const std::string& line) {
    std::vector<std::string> cols;
    size_t i = 0, n = line.size();
    while (i < n) {
        while (i < n && line[i] == ' ') ++i;
        if (i >= n) break;
        size_t start = i;
        size_t last_char = i;
        while (i < n) {
            if (line[i] == ' ') {
                size_t j = i;
                while (j < n && line[j] == ' ') ++j;
                if (j - i >= 2 || j >= n) break;
                i = j;
            } else {
                last_char = i;
                ++i;
            }
        }
        cols.push_back(rtrim(line.substr(start, last_char - start + 1)));
    }
    return cols;
}
bool parse_number(std::string text, std::string& digits, long& ls_exp) {
    std::string t;
    for (char c : text) if (c != ' ') t += c;
    size_t dots = t.find("...");
    if (dots != std::string::npos) t.erase(dots, 3);
    if (!t.empty() && (t[0] == '-' || t[0] == '+')) t.erase(0, 1);
    long e = 0;
    size_t epos = t.find('e');
    if (epos != std::string::npos) {
        try { e = std::stol(t.substr(epos + 1)); } catch (...) { return false; }
        t = t.substr(0, epos);
    }
    long frac = 0;
    size_t dpos = t.find('.');
    if (dpos != std::string::npos) {
        frac = (long)(t.size() - dpos - 1);
        t.erase(dpos, 1);
    }
    if (t.empty()) return false;
    for (char c : t) if (c < '0' || c > '9') return false;
    digits = strip_leading_zeros(t);
    ls_exp = e - frac;
    return true;
}
std::string add_digits(const std::string& a, const std::string& b) {
    std::string r;
    int carry = 0, i = (int)a.size() - 1, j = (int)b.size() - 1;
    while (i >= 0 || j >= 0 || carry) {
        int s = carry;
        if (i >= 0) s += a[i--] - '0';
        if (j >= 0) s += b[j--] - '0';
        r.push_back('0' + s % 10);
        carry = s / 10;
    }
    std::reverse(r.begin(), r.end());
    return r;
}
int cmp_digits(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    return a.compare(b);
}
std::string sub_digits(const std::string& a, const std::string& b) {
    std::string r;
    int borrow = 0, i = (int)a.size() - 1, j = (int)b.size() - 1;
    while (i >= 0) {
        int d = (a[i] - '0') - borrow - (j >= 0 ? b[j] - '0' : 0);
        if (d < 0) { d += 10; borrow = 1; } else borrow = 0;
        r.push_back('0' + d);
        --i; --j;
    }
    while (r.size() > 1 && r.back() == '0') r.pop_back();
    std::reverse(r.begin(), r.end());
    return r;
}
void compute_certain(Constant& c, const std::string& unc_digits, long unc_ls, bool exact) {
    c.offset = (long)c.digits.size() + c.ls_exp;
    if (exact) { c.exact = true; c.certain = c.digits; return; }
    long common_ls = std::min(c.ls_exp, unc_ls);
    std::string vd = c.digits + std::string(c.ls_exp - common_ls, '0');
    std::string ud = strip_leading_zeros(unc_digits) + std::string(unc_ls - common_ls, '0');
    if (cmp_digits(vd, ud) <= 0) { c.certain = ""; return; }
    std::string hi = add_digits(vd, ud);
    std::string lo = sub_digits(vd, ud);
    if (hi.size() != lo.size()) { c.certain = ""; return; }
    size_t k = 0;
    while (k < hi.size() && hi[k] == lo[k]) ++k;
    c.certain = hi.substr(0, std::min(k, c.digits.size()));
}
std::vector<Constant> parse_nist(const std::string& path, bool& ok, std::string& problem) {
    std::vector<Constant> out;
    std::ifstream file(path);
    ok = false;
    if (!file) { problem = "cannot open NIST table"; return out; }
    std::string line;
    bool in_data = false;
    while (std::getline(file, line)) {
        std::string t = rtrim(line);
        if (!in_data) {
            if (t.size() > 20 && t.find_first_not_of('-') == std::string::npos) in_data = true;
            continue;
        }
        if (t.empty()) continue;
        auto cols = split_columns(t);
        if (cols.size() < 3) continue;
        Constant c;
        c.name = cols[0];
        c.unit = cols.size() >= 4 ? cols[3] : "";
        if (!parse_number(cols[1], c.digits, c.ls_exp)) continue;
        bool exact = cols[2].find("exact") != std::string::npos;
        std::string ud;
        long uls = 0;
        if (!exact && !parse_number(cols[2], ud, uls)) continue;
        compute_certain(c, ud, uls, exact);
        out.push_back(c);
    }
    if (!in_data) { problem = "table header not found; format may have changed"; return out; }
    if (out.size() < MIN_NIST_ROWS) {
        problem = "parsed only " + std::to_string(out.size()) +
                  " constants (expected " + std::to_string(MIN_NIST_ROWS) +
                  "+); table format may have changed";
        return out;
    }
    bool light_ok = false;
    for (const auto& c : out) {
        if (c.name == "speed of light in vacuum") {
            light_ok = c.exact && c.digits == "299792458";
            break;
        }
    }
    if (!light_ok) {
        problem = "sanity probe failed (speed of light not parsed as exactly 299792458); "
                  "table format may have changed and the parser needs updating";
        return out;
    }
    ok = true;
    return out;
}
std::string read_first_line(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    if (file) std::getline(file, line);
    return rtrim(line);
}
std::string payload_of(const std::string& line) {
    size_t sp1 = line.find(' ');
    if (sp1 == std::string::npos) return "";
    size_t sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return line.substr(sp1 + 1);
    return line.substr(sp2 + 1);
}
bool has_cons_keyword(const std::string& kline) {
    std::stringstream ss(kline);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (rtrim(tok) == "cons") return true;
    }
    return false;
}
bool build_index(const std::string& seq_dir, const std::string& index_path,
                 const std::string& time_key) {
    std::ofstream out(index_path);
    if (!out) return false;
    out << INDEX_FORMAT << "\n";
    out << "TIME\t" << time_key << "\n";
    size_t scanned = 0, kept = 0, flagged = 0;
    std::error_code ec;
    fs::recursive_directory_iterator it(seq_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return false;
    for (auto& entry : it) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".seq") continue;
        ++scanned;
        if (scanned % 50000 == 0) std::cout << " scanned " << scanned << " files...\n";
        std::ifstream f(entry.path());
        if (!f) continue;
        std::string line, data, name, kw;
        long offset = 0;
        bool has_offset = false;
        while (std::getline(f, line)) {
            if (line.size() < 3 || line[0] != '%') continue;
            char code = line[1];
            if (code == 'S' || code == 'T' || code == 'U') data += payload_of(line);
            else if (code == 'N') name = rtrim(payload_of(line));
            else if (code == 'K') kw = rtrim(payload_of(line));
            else if (code == 'O') {
                try { offset = std::stol(payload_of(line)); has_offset = true; } catch (...) {}
            }
        }
        bool kw_cons = !kw.empty() && has_cons_keyword(kw);
        bool name_dec = lower(name).find("decimal expansion") != std::string::npos;
        if (data.empty() || !has_offset) continue;
        std::string digits;
        bool regular = true;
        std::stringstream ds(data);
        std::string term;
        while (std::getline(ds, term, ',')) {
            term = rtrim(term);
            size_t s = term.find_first_not_of(" \t");
            if (s != std::string::npos) term = term.substr(s);
            if (term.empty()) continue;
            if (term.size() != 1 || term[0] < '0' || term[0] > '9') { regular = false; break; }
            digits += term;
        }
        if (digits.size() < MIN_MATCH) continue;
        if (!kw_cons && !name_dec && !regular) continue;
        int flags = 0;
        if (kw_cons) flags |= 1;
        if (!regular) { flags |= 2; ++flagged; }
        if (name_dec) flags |= 4;
        out << entry.path().stem().string() << "\t" << flags << "\t" << offset
            << "\t" << digits << "\t" << name << "\n";
        ++kept;
    }
    std::cout << "Indexed " << kept << " decimal-expansion sequences out of " << scanned
              << " files (" << flagged << " with irregular data).\n";
    return true;
}
std::vector<ConsSeq> load_index(const std::string& index_path, const std::string& time_key,
                                bool& valid) {
    std::vector<ConsSeq> out;
    valid = false;
    std::ifstream file(index_path);
    if (!file) return out;
    std::string line;
    if (!std::getline(file, line) || rtrim(line) != INDEX_FORMAT) return out;
    if (!std::getline(file, line) || rtrim(line) != "TIME\t" + time_key) return out;
    while (std::getline(file, line)) {
        size_t t1 = line.find('\t');
        size_t t2 = (t1 == std::string::npos) ? std::string::npos : line.find('\t', t1 + 1);
        size_t t3 = (t2 == std::string::npos) ? std::string::npos : line.find('\t', t2 + 1);
        size_t t4 = (t3 == std::string::npos) ? std::string::npos : line.find('\t', t3 + 1);
        if (t4 == std::string::npos) continue;
        ConsSeq s;
        s.anum = line.substr(0, t1);
        int flags = 0;
        try {
            flags = std::stoi(line.substr(t1 + 1, t2 - t1 - 1));
            s.offset = std::stol(line.substr(t2 + 1, t3 - t2 - 1));
        } catch (...) { continue; }
        s.has_kw = (flags & 1) != 0;
        s.malformed = (flags & 2) != 0;
        s.has_name = (flags & 4) != 0;
        s.digits = line.substr(t3 + 1, t4 - t3 - 1);
        s.name = rtrim(line.substr(t4 + 1));
        out.push_back(s);
    }
    valid = true;
    return out;
}
size_t lcp_len(const std::string& a, const std::string& b) {
    size_t n = std::min(a.size(), b.size()), i = 0;
    while (i < n && a[i] == b[i]) ++i;
    return i;
}
std::string utc_now(const char* fmt) {
    std::time_t t = std::time(nullptr);
    std::tm* g = std::gmtime(&t);
    char buf[64];
    std::strftime(buf, sizeof(buf), fmt, g);
    return std::string(buf);
}
std::string build_report(const std::vector<Constant>& constants,
                         const std::vector<ConsSeq>& seqs) {
    std::ostringstream updates, fixes, missing;
    size_t ok_count = 0, upd_count = 0, fix_count = 0, miss_count = 0,
           skip_count = 0, malformed_count = 0;
    for (const auto& c : constants) {
        if (c.certain.size() < MIN_MATCH) { ++skip_count; continue; }
        const ConsSeq* best = nullptr;
        size_t best_lcp = 0;
        const ConsSeq* best_untag = nullptr;
        size_t best_untag_lcp = 0;
        const ConsSeq* best_mal = nullptr;
        size_t best_mal_lcp = 0;
        for (const auto& s : seqs) {
            size_t l = lcp_len(c.certain, s.digits);
            bool tagged = s.has_kw || s.has_name;
            if (s.malformed) {
                if (l > best_mal_lcp) { best_mal_lcp = l; best_mal = &s; }
            } else if (tagged) {
                if (l > best_lcp) { best_lcp = l; best = &s; }
            } else {
                if (l > best_untag_lcp) { best_untag_lcp = l; best_untag = &s; }
            }
        }
        // Untagged sequences may never claim an exact constant
        if ((!best || best_lcp < MIN_MATCH) &&
            best_untag && best_untag_lcp >= STRONG_MATCH && !c.exact) {
            best = best_untag;
            best_lcp = best_untag_lcp;
        }
        bool mal_hit = best_mal && best_mal_lcp >= MIN_MATCH;
        if (mal_hit) {
            fixes << best_mal->anum << " " << c.name
                  << " | matches this constant but data terms are not single decimal digits\n";
            ++fix_count;
            ++malformed_count;
        }
        if (!best || best_lcp < MIN_MATCH) {
            if (mal_hit) continue;
            missing << c.name << " [" << c.certain.size() << " certain digits: "
                    << c.certain << "]";
            if (!c.unit.empty()) missing << " " << c.unit;
            missing << "\n";
            ++miss_count;
            continue;
        }
        bool flagged = false;
        if (best_lcp == best->digits.size() && best->digits.size() < c.certain.size()) {
            updates << best->anum << " " << c.name << " | has "
                    << best->digits.size() << " digits, certain to "
                    << c.certain.size() << ", append: "
                    << c.certain.substr(best->digits.size()) << "\n";
            ++upd_count;
            flagged = true;
        } else if (best_lcp < c.certain.size() && best_lcp < best->digits.size()) {
            fixes << best->anum << " " << c.name << " | digit mismatch at position "
                  << (best_lcp + 1) << ": seq '" << best->digits[best_lcp]
                  << "' vs CODATA '" << c.certain[best_lcp] << "'\n";
            ++fix_count;
            flagged = true;
        }
        if (best->offset != c.offset) {
            fixes << best->anum << " " << c.name << " | offset " << best->offset
                  << " vs expected " << c.offset << "\n";
            ++fix_count;
            flagged = true;
        }
        if (!best->has_kw) {
            fixes << best->anum << " " << c.name
                  << " | matches this constant but lacks the 'cons' keyword\n";
            ++fix_count;
            flagged = true;
        }
        if (!flagged) ++ok_count;
    }
    std::ostringstream r;
    r << "TIMESTAMP UTC " << utc_now("%Y-%m-%d %H:%M:%S") << "\n\n";
    r << "OEIS SEQUENCES NEEDING UPDATE:\n\n";
    r << (upd_count ? updates.str() : "(none)\n");
    r << "\nOEIS SEQUENCES NEEDING FIX:\n\n";
    r << (fix_count ? fixes.str() : "(none)\n");
    r << "\nOEIS MISSING CONSTANTS:\n\n";
    r << (miss_count ? missing.str() : "(none)\n");
    r << "\nSUMMARY: " << ok_count << " consistent, " << upd_count << " need update, "
      << fix_count << " need fix (" << malformed_count << " malformed), "
      << miss_count << " missing, "
      << skip_count << " skipped (fewer than " << MIN_MATCH << " certain digits), "
      << seqs.size() << " sequences indexed.\n";
    return r.str();
}
}
CommandRegistrar reg_run("run", []() {
    std::string sub = current_args().empty() ? "" : current_args()[0];
    if (sub != "" && sub != "export" && sub != "index") {
        std::cerr << "\033[31mError: unknown run subcommand \"" << sub
                  << "\". Use: run, run export, run index.\033[0m\n";
        return true;
    }
    std::string home = home_dir();
    std::string nist_path = home + "/oeis/cons/allascii.txt";
    std::string seq_dir = home + "/oeis/oeisdata/seq";
    std::string time_path = home + "/oeis/oeisdata/time.txt";
    std::string index_path = home + "/oeis/cons/cons.index";
    if (!fs::exists(seq_dir)) {
        std::cerr << "\033[31mError: oeisdata not found. Run fetch first.\033[0m\n";
        return true;
    }
    if (!fs::exists(nist_path)) {
        std::cerr << "\033[31mError: NIST table not found. Run pull first.\033[0m\n";
        return true;
    }
    std::string time_key = read_first_line(time_path);
    if (sub == "index") {
        std::cout << "Rebuilding index...\n";
        if (!build_index(seq_dir, index_path, time_key)) {
            std::cerr << "\033[31mError: could not build index.\033[0m\n";
        }
        return true;
    }
    bool index_valid = false;
    std::vector<ConsSeq> seqs = load_index(index_path, time_key, index_valid);
    if (!index_valid) {
        std::cout << "Index missing, stale, or from an older format. Rebuilding...\n";
        if (!build_index(seq_dir, index_path, time_key)) {
            std::cerr << "\033[31mError: could not build index.\033[0m\n";
            return true;
        }
        seqs = load_index(index_path, time_key, index_valid);
        if (!index_valid) {
            std::cerr << "\033[31mError: index rebuild failed.\033[0m\n";
            return true;
        }
    }
    bool nist_ok = false;
    std::string nist_problem;
    std::vector<Constant> constants = parse_nist(nist_path, nist_ok, nist_problem);
    if (!nist_ok) {
        std::cerr << "\033[31mError: " << nist_problem << ". Re-run pull, or update the parser.\033[0m\n";
        return true;
    }
    std::string report = build_report(constants, seqs);
    if (sub == "export") {
        std::string out_path = home + "/oeis/cons/report_" + utc_now("%Y%m%d_%H%M%S") + ".txt";
        std::ofstream out(out_path);
        if (!out) {
            std::cerr << "\033[31mError: could not write report file.\033[0m\n";
            return true;
        }
        out << report;
        std::cout << "Report exported: " << out_path << "\n";
    } else {
        std::cout << report;
    }
    return true;
});
