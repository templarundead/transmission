// This file Copyright © 2022-2023 Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include <algorithm> // for std::copy_n
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <event2/buffer.h>

#include "error.h"
#include "net.h" // tr_socket_t
#include "tr-assert.h"
#include "utils-ev.h"
#include "utils.h" // for tr_htonll(), tr_ntohll()

namespace libtransmission
{

template<typename value_type>
class BufferReader
{
public:
    virtual ~BufferReader() = default;
    virtual void drain(size_t n_bytes) = 0;
    [[nodiscard]] virtual size_t size() const noexcept = 0;
    [[nodiscard]] virtual value_type const* data() const = 0;

    [[nodiscard]] auto empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]] auto const* begin() const noexcept
    {
        return data();
    }

    [[nodiscard]] auto const* end() const noexcept
    {
        return begin() + size();
    }

    template<typename T>
    [[nodiscard]] TR_CONSTEXPR20 bool starts_with(T const& needle) const
    {
        auto const n_bytes = std::size(needle);
        auto const needle_begin = reinterpret_cast<std::byte const*>(std::data(needle));
        auto const needle_end = needle_begin + n_bytes;
        return n_bytes <= size() && std::equal(needle_begin, needle_end, data());
    }

    [[nodiscard]] auto to_string_view() const
    {
        return std::string_view{ reinterpret_cast<char const*>(data()), size() };
    }

    [[nodiscard]] auto to_string() const
    {
        return std::string{ to_string_view() };
    }

    void to_buf(void* tgt, size_t n_bytes)
    {
        n_bytes = std::min(n_bytes, size());
        std::copy_n(data(), n_bytes, static_cast<value_type*>(tgt));
        drain(n_bytes);
    }

    [[nodiscard]] auto to_uint8()
    {
        auto tmp = uint8_t{};
        to_buf(&tmp, sizeof(tmp));
        return tmp;
    }

    [[nodiscard]] uint16_t to_uint16()
    {
        auto tmp = uint16_t{};
        to_buf(&tmp, sizeof(tmp));
        return ntohs(tmp);
    }

    [[nodiscard]] uint32_t to_uint32()
    {
        auto tmp = uint32_t{};
        to_buf(&tmp, sizeof(tmp));
        return ntohl(tmp);
    }

    [[nodiscard]] uint64_t to_uint64()
    {
        auto tmp = uint64_t{};
        to_buf(&tmp, sizeof(tmp));
        return tr_ntohll(tmp);
    }

    // Returns the number of bytes written. Check `error` for error.
    size_t to_socket(tr_socket_t sockfd, size_t n_bytes, tr_error** error = nullptr)
    {
        n_bytes = std::min(n_bytes, size());

        if (n_bytes == 0U)
        {
            return {};
        }

        if (auto const n_sent = send(sockfd, reinterpret_cast<char const*>(data()), n_bytes, 0); n_sent >= 0U)
        {
            drain(n_sent);
            return n_sent;
        }

        auto const err = sockerrno;
        tr_error_set(error, err, tr_net_strerror(err));
        return {};
    }
};

template<typename value_type>
class BufferWriter
{
public:
    virtual ~BufferWriter() = default;
    virtual std::pair<value_type*, size_t> reserve_space(size_t n_bytes) = 0;
    virtual void commit_space(size_t n_bytes) = 0;

    void add(void const* span_begin, size_t span_len)
    {
        auto [buf, buflen] = reserve_space(span_len);
        std::copy_n(reinterpret_cast<value_type const*>(span_begin), span_len, buf);
        commit_space(span_len);
    }

    template<typename ContiguousContainer>
    void add(ContiguousContainer const& container)
    {
        add(std::data(container), std::size(container));
    }

    template<typename OneByteType>
    void push_back(OneByteType ch)
    {
        add(&ch, 1);
    }

    void add_uint8(uint8_t uch)
    {
        add(&uch, 1);
    }

    void add_uint16(uint16_t hs)
    {
        uint16_t const ns = htons(hs);
        add(&ns, sizeof(ns));
    }

    void add_hton16(uint16_t hs)
    {
        add_uint16(hs);
    }

    void add_uint32(uint32_t hl)
    {
        uint32_t const nl = htonl(hl);
        add(&nl, sizeof(nl));
    }

    void eadd_hton32(uint32_t hl)
    {
        add_uint32(hl);
    }

    void add_uint64(uint64_t hll)
    {
        uint64_t const nll = tr_htonll(hll);
        add(&nll, sizeof(nll));
    }

    void add_hton64(uint64_t hll)
    {
        add_uint64(hll);
    }

    void add_port(tr_port port)
    {
        auto nport = port.network();
        add(&nport, sizeof(nport));
    }
};

class Buffer final
    : public BufferReader<std::byte>
    , public BufferWriter<std::byte>
{
public:
    using value_type = std::byte;

    Buffer() = default;
    Buffer(Buffer&&) = default;
    Buffer(Buffer const&) = delete;
    Buffer& operator=(Buffer&&) = default;
    Buffer& operator=(Buffer const&) = delete;

    template<typename T>
    explicit Buffer(T const& data)
    {
        add(data);
    }

    // -- BufferReader

    [[nodiscard]] size_t size() const noexcept override
    {
        return evbuffer_get_length(buf_.get());
    }

    [[nodiscard]] value_type const* data() const override
    {
        return reinterpret_cast<std::byte*>(evbuffer_pullup(buf_.get(), -1));
    }

    void drain(size_t n_bytes) override
    {
        evbuffer_drain(buf_.get(), n_bytes);
    }

    // -- BufferWriter

    [[nodiscard]] std::pair<value_type*, size_t> reserve_space(size_t n_bytes) override
    {
        auto iov = evbuffer_iovec{};
        evbuffer_reserve_space(buf_.get(), n_bytes, &iov, 1);
        TR_ASSERT(iov.iov_len >= n_bytes);
        reserved_space_ = iov;
        return { static_cast<value_type*>(iov.iov_base), static_cast<size_t>(iov.iov_len) };
    }

    void commit_space(size_t n_bytes) override
    {
        TR_ASSERT(reserved_space_);
        TR_ASSERT(reserved_space_->iov_len >= n_bytes);
        reserved_space_->iov_len = n_bytes;
        evbuffer_commit_space(buf_.get(), &*reserved_space_, 1);
        reserved_space_.reset();
    }

    //

    size_t add_socket(tr_socket_t sockfd, size_t n_bytes, tr_error** error = nullptr)
    {
        EVUTIL_SET_SOCKET_ERROR(0);
        auto const res = evbuffer_read(buf_.get(), sockfd, static_cast<int>(n_bytes));
        auto const err = EVUTIL_SOCKET_ERROR();

        if (res > 0)
        {
            return static_cast<size_t>(res);
        }

        if (res == 0)
        {
            tr_error_set_from_errno(error, ENOTCONN);
        }
        else
        {
            tr_error_set(error, err, tr_net_strerror(err));
        }

        return {};
    }

private:
    evhelpers::evbuffer_unique_ptr buf_{ evbuffer_new() };
    std::optional<evbuffer_iovec> reserved_space_;
};

} // namespace libtransmission
