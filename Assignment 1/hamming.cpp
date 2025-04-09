#include <bits/stdc++.h>
// using i64 = long long;
// #define int i64
#define pb push_back
#define ep emplace
#define eb emplace_back
using namespace std;
template <class T1, class T2> ostream &operator<<(ostream &os, const std::pair<T1, T2> &a) { return os << "(" << a.first << ", " << a.second << ")"; };
template <class T> ostream &operator<<(ostream &os, const vector<T> &as) { const int sz = as.size(); os << "["; for (int i = 0; i < sz; ++i) { if (i >= 256) { os << ", ..."; break; } if (i > 0) { os << ", "; } os << as[i]; } return os << "]"; }
template <class T> void pv(T a, T b) { for (T i = a; i != b; ++i) cerr << *i << " "; cerr << '\n'; }
using pii = std::pair<int, int>;
#define fi first
#define se second

void solve() {
    string s;
    // cin >> s;
    s = "111111100110";
    // s = "110110101010";
    
    int n = s.size();

    int m = 1;
    while ((1 << m) < n + m + 1) m++;
    
    
    vector<int> a(n + 1, 0);
    for (int i = 1; i <= n; i++) a[i] = s[i - 1] - '0';
    cerr << a << '\n';
    
    int p = 1;
    int len = n + m;
    vector<int> b(len + 1, 0);
    for (int i = 1; i <= len; i++) {
        if (__builtin_popcount(i) == 1) {
            cerr << i << '\n';
            continue;
        }
            
        b[i] = a[p++];
        for (int j = 1; j <= i; j <<= 1) {
            if (i & j) {
                if (j == 8) {
                    cout << i << ' ' << a[i] << '\n';
                }
                b[j] ^= b[i];
            }
        }
    }    
    
    
    cerr << len << '\n';
    
    for (int i = 1; i <= len; i++) {
        cout << b[i];
        if (i % 4 == 0) cout << " ";
    }
    cout << '\n';
}

signed main() {
    std::ios::sync_with_stdio(0);
    std::cin.tie(0);
    // int T; cin >> T;
    // for (; T--; solve());
    solve();
    return 0;
}