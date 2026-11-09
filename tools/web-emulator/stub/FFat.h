// PC / ブラウザ用の FFat の代わり(メモリの中だけのファイル置き場)
#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdint>
#include <algorithm>

struct FsNode { std::vector<uint8_t> data; bool dir = false; };
static std::map<std::string, std::shared_ptr<FsNode>>& fsMap() { static std::map<std::string, std::shared_ptr<FsNode>> m; return m; }
static bool   g_fsAvail = true;                 // false にすると「FATFS の区画がない」状態
static size_t g_fsTotal = 10125312;             // 本物の 9.9MB の区画で、FAT が使える量くらい

class File {
  std::shared_ptr<FsNode> n; std::string path, nm; size_t pos = 0;
  std::vector<std::string> kids; size_t ki = 0; bool listed = false;
public:
  File() {}
  File(std::shared_ptr<FsNode> node, const std::string& p) : n(node), path(p) {
    size_t s = p.rfind('/'); nm = s == std::string::npos ? p : p.substr(s + 1);
  }
  explicit operator bool() const { return (bool)n; }
  size_t read(uint8_t* b, size_t len) { if (!n) return 0; size_t k = std::min(len, n->data.size() - std::min(pos, n->data.size())); if (k) memcpy(b, n->data.data() + pos, k); pos += k; return k; }
  size_t write(const uint8_t* b, size_t len) {
    if (!n) return 0;
    if (n->data.size() < pos + len) n->data.resize(pos + len);
    memcpy(n->data.data() + pos, b, len); pos += len; return len;
  }
  bool seek(uint32_t p) { if (!n) return false; pos = std::min((size_t)p, n->data.size()); return true; }
  size_t position() const { return pos; }
  size_t size() const { return n ? n->data.size() : 0; }
  void close() { n.reset(); }
  bool isDirectory() const { return n && n->dir; }
  const char* name() const { return nm.c_str(); }
  const char* path_() const { return path.c_str(); }
  File openNextFile(const char* = "r") {
    if (!n || !n->dir) return File();
    if (!listed) {
      listed = true; std::string pre = path + "/";
      for (auto& kv : fsMap()) if (kv.first.compare(0, pre.size(), pre) == 0 && kv.first.find('/', pre.size()) == std::string::npos) kids.push_back(kv.first);
    }
    if (ki >= kids.size()) return File();
    const std::string& k = kids[ki++];
    return File(fsMap()[k], k);
  }
};

struct FFatFS {
  bool begin(bool = false, const char* = "/ffat", uint8_t = 10, const char* = "ffat") { return g_fsAvail; }
  size_t totalBytes() { return g_fsTotal; }
  size_t usedBytes() {                          // 4KB ごとの区切りで数える(本物の FAT と同じように)
    size_t u = 0; for (auto& kv : fsMap()) u += kv.second->dir ? 4096 : (kv.second->data.size() + 4095) / 4096 * 4096;
    return u;
  }
  size_t freeBytes() { size_t u = usedBytes(); return g_fsTotal > u ? g_fsTotal - u : 0; }
  File open(const char* p, const char* mode = "r", bool = false) {
    auto& m = fsMap(); std::string k(p);
    if (mode[0] == 'w') { auto node = std::make_shared<FsNode>(); m[k] = node; return File(node, k); }
    auto it = m.find(k); if (it == m.end()) return File();
    return File(it->second, k);
  }
  bool exists(const char* p) { return fsMap().count(p) > 0; }
  bool remove(const char* p) { return fsMap().erase(p) > 0; }
  bool rename(const char* a, const char* b) {
    auto& m = fsMap(); auto it = m.find(a); if (it == m.end()) return false;
    auto node = it->second; m.erase(it); m[b] = node; return true;
  }
  bool mkdir(const char* p) { auto node = std::make_shared<FsNode>(); node->dir = true; fsMap()[p] = node; return true; }
};
static FFatFS FFat;
// ブラウザ・テストから、ファイルを直接入れる
static void fsPut(const char* path, const uint8_t* d, size_t n) {
  auto node = std::make_shared<FsNode>(); node->data.assign(d, d + n); fsMap()[path] = node;
}
