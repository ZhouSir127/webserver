#ifndef _CRYPTO_UTIL_H_
#define _CRYPTO_UTIL_H_

#include <string>

namespace cryptoUtil {
    // 1. 生成高强度随机盐 (默认 16 字符)
    std::string generateSalt(int length = 16);

    // 2. SHA-256 哈希计算
    std::string sha256(const std::string& input);

    // 3. 将 密码与盐 结合并进行哈希
    std::string hashPassword(const std::string& password, const std::string& salt);
}

#endif