#pragma once

/**
 * @file ByteStream.h
 * @brief 명령 직렬화에 쓰는 바이트 쓰기/읽기 도우미
 */

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

/**
 * @brief 값들을 바이트 버퍼 끝에 차례로 붙입니다. (리틀 엔디언, 같은 플랫폼 간 교환용)
 */
class ByteWriter
{
public:
    /** @brief 단순 복사 가능한 값 하나를 씁니다. */
    template <typename T>
    void Write(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "trivially copyable 형식만 쓸 수 있습니다.");
        const auto* p = reinterpret_cast<const uint8_t*>(&value);
        m_buffer.insert(m_buffer.end(), p, p + sizeof(T));
    }

    /** @brief 원소 개수(uint32)와 원소들을 씁니다. */
    template <typename T>
    void WriteVector(const std::vector<T>& values)
    {
        static_assert(std::is_trivially_copyable_v<T>, "trivially copyable 형식만 쓸 수 있습니다.");
        Write(static_cast<uint32_t>(values.size()));
        if (!values.empty())
        {
            const auto* p = reinterpret_cast<const uint8_t*>(values.data());
            m_buffer.insert(m_buffer.end(), p, p + values.size() * sizeof(T));
        }
    }

    /** @brief 지금까지 쓴 버퍼 */
    const std::vector<uint8_t>& Buffer() const { return m_buffer; }

private:
    std::vector<uint8_t> m_buffer;
};

/**
 * @brief 바이트 버퍼에서 값을 차례로 읽습니다. 데이터가 모자라면 std::runtime_error를 던집니다.
 */
class ByteReader
{
public:
    ByteReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    /** @brief 단순 복사 가능한 값 하나를 읽습니다. */
    template <typename T>
    T Read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "trivially copyable 형식만 읽을 수 있습니다.");
        Require(sizeof(T));
        T value;
        std::memcpy(&value, m_data + m_pos, sizeof(T));
        m_pos += sizeof(T);
        return value;
    }

    /**
     * @brief WriteVector로 쓴 배열을 읽습니다.
     * @param maxCount 허용할 최대 원소 수 (손상된 데이터로 큰 메모리를 잡지 않도록)
     */
    template <typename T>
    std::vector<T> ReadVector(uint32_t maxCount)
    {
        const auto count = Read<uint32_t>();
        if (count > maxCount)
        {
            throw std::runtime_error("ByteReader: 원소 수가 허용 범위를 넘었습니다.");
        }
        Require(static_cast<size_t>(count) * sizeof(T));
        std::vector<T> values(count);
        if (count > 0)
        {
            std::memcpy(values.data(), m_data + m_pos, count * sizeof(T));
        }
        m_pos += count * sizeof(T);
        return values;
    }

    /** @brief 모두 읽었는지 */
    bool AtEnd() const { return m_pos == m_size; }

private:
    void Require(size_t bytes) const
    {
        if (bytes > m_size - m_pos)
        {
            throw std::runtime_error("ByteReader: 데이터가 부족합니다.");
        }
    }

    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_pos = 0;
};
