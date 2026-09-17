#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>

bool load_libsodium();
std::string hash_string(std::string string_to_hash);
bool verify_string(const std::string& hashed, const std::string& plain);

#endif // CRYPTO_H
