#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>

bool load_libsodium();
std::string hash_string(std::string string_to_hash);

#endif // CRYPTO_H
