#ifndef BIG_INT_H
#define BIG_INT_H
#include <string>

// 极简大数类：string存储、默认0、仅支持++a、获取字符串
class BigInt {
private:
    // 核心存储：默认初始化为 "0"
    std::string m_num{"0"};

public:
    // -------------------------- 核心：前置自增 ++a --------------------------
    BigInt& operator++() {
        int i = m_num.size() - 1;
        int carry = 1;  // 自增 1

        // 从最后一位开始处理进位
        while (i >= 0 && carry) {
            int digit = (m_num[i] - '0') + carry;
            carry = digit / 10;
            m_num[i] = (digit % 10) + '0';
            --i;
        }

        // 最高位进位（如 999 → 1000）
        if (carry) {
            m_num.insert(m_num.begin(), '1');
        }
        return *this;
    }

    // -------------------------- 唯一对外接口：获取内部字符串 --------------------------
    std::string str() const {
        return m_num;
    }
};

#endif