#include <iostream>
#include <vector>
#include <set>

using namespace std;
// int main() {
//     int N, M;
//     cin >> N >> M;
//     vector<vector<pair<int, int>>> adj(N+1);
//     vector<int> dis(N+1);
//     for(int i = 0; i < M; i++) {
//         int u, v, w;
//         cin >> u >> v >> w;
//         adj[u].push_back({v, w});
//         adj[v].push_back({u, w});
//     }


//     set<pair<int, int>> s;
//     s.insert({1, 0});
//     for(int i = 2; i <= N; i++) {
//         dis[i] = 1e9;
//         s.insert({i, dis[i]});
//     }

//     while(!s.empty()) {
//         auto [d, n] = *s.begin();
//         s.erase({d, n});
//         for(auto [next, w] : adj[n]) {
//             if(dis[n] + w < dis[next]) {
//                 s.erase({dis[next], next});
//                 dis[next] = dis[n] + w;
//                 s.insert({dis[next], next});
//             }
//         }
//     }

//     for(int i = 1; i <= N; i++) cout << (dis[i] == 1e9 ? -1 : dis[i]) << endl;

// }
void f(vector<int> v, int x) {
    if(x == v.size()) return;
    cout << v[x] << endl;
    f(v, x+1);
}

int main() {

    vector<int> v(1000);
    f(v, 0);
}