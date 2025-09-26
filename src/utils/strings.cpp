#include "strings.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <vector>

namespace karing::str {

std::string utf8_prefix(const std::string& s, size_t max_bytes) {
  if (s.size() <= max_bytes) return s;
  size_t i = 0, last = 0;
  while (i < s.size() && i < max_bytes) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t adv = 1;
    if ((c & 0x80u) == 0) adv = 1;
    else if ((c & 0xE0u) == 0xC0u) adv = 2;
    else if ((c & 0xF0u) == 0xE0u) adv = 3;
    else if ((c & 0xF8u) == 0xF0u) adv = 4;
    else break; // invalid; stop
    if (i + adv > max_bytes) break;
    last = i + adv;
    i += adv;
  }
  return s.substr(0, last);
}

std::string sha256_hex(const void* data, size_t len) {
  unsigned char md[EVP_MAX_MD_SIZE]; unsigned int mdlen = 0;
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) return {};
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) { EVP_MD_CTX_free(ctx); return {}; }
  if (EVP_DigestUpdate(ctx, data, len) != 1) { EVP_MD_CTX_free(ctx); return {}; }
  if (EVP_DigestFinal_ex(ctx, md, &mdlen) != 1) { EVP_MD_CTX_free(ctx); return {}; }
  EVP_MD_CTX_free(ctx);
  static const char* hex = "0123456789abcdef";
  std::string out; out.resize(mdlen * 2);
  for (unsigned int i = 0; i < mdlen; ++i) {
    out[i*2] = hex[(md[i] >> 4) & 0xF];
    out[i*2+1] = hex[md[i] & 0xF];
  }
  return out;
}

std::string simple_diff(const std::string& before, const std::string& after, size_t max_output) {
  // limit size
  if (before.size() > 1<<20 || after.size() > 1<<20) return {};
  // split by lines
  std::vector<std::string> a, b;
  {
    size_t pos=0, n=before.size();
    while (pos<=n) { size_t q=before.find('\n', pos); if (q==std::string::npos) q=n; a.emplace_back(before.substr(pos, q-pos)); pos=q+1; }
  }
  {
    size_t pos=0, n=after.size();
    while (pos<=n) { size_t q=after.find('\n', pos); if (q==std::string::npos) q=n; b.emplace_back(after.substr(pos, q-pos)); pos=q+1; }
  }
  size_t i=0; while (i<a.size() && i<b.size() && a[i]==b[i]) ++i; // common prefix
  size_t ai=a.size(), bi=b.size();
  while (ai>i && bi>i && a[ai-1]==b[bi-1]) { --ai; --bi; } // common suffix
  std::string out;
  for (size_t k=i; k<ai && out.size()<max_output; ++k) {
    out += "-"; out += a[k]; out += "\n";
  }
  for (size_t k=i; k<bi && out.size()<max_output; ++k) {
    out += "+"; out += b[k]; out += "\n";
  }
  if (out.size()>max_output) out.resize(max_output);
  return out;
}

}
