#include"crypto_util.h"

#include "crypto_util.h"
#include <openssl/evp.h>
#include <random>
#include <cstdio>

std::string cryptoUtil::generateSalt(int length) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_int_distribution<int> dis(0, 61);

    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";//总共62个字符
    std::string salt;
    for (int i = 0; i < length; ++i)
        salt += charset[dis(gen)];

    return salt;
}//62个字符随机生成salt

std::string cryptoUtil::sha256(const std::string& input) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
    EVP_DigestUpdate(context, input.c_str(), input.length());
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;
    EVP_DigestFinal_ex(context, hash, &lengthOfHash);
    EVP_MD_CTX_free(context);

    // 将二进制 Hash 转换为 16 进制字符串
    char buf[65]; // SHA-256 是 32 字节，16进制表示需 64 字符 + '\0'
    for (unsigned int i = 0; i < lengthOfHash; ++i) {
        snprintf(buf + (i * 2), 3, "%02x", hash[i]);
    }
    return std::string(buf);
}

std::string cryptoUtil::hashPassword(const std::string& password, const std::string& salt) {
    // 工业界常用做法：password拼接salt后进行哈希
    return sha256(password + salt);
}