// This file Copyright © 2023-2023 Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include <libtransmission/global-ip-cache.h>
#include <libtransmission/net.h>
#include <libtransmission/timer.h>
#include <libtransmission/web.h>

#include "gtest/gtest.h"

using namespace std::literals;

class GlobalIPCacheTest : public ::testing::Test
{
protected:
    class MockTimerMaker final : public libtransmission::TimerMaker
    {
    public:
        [[nodiscard]] std::unique_ptr<libtransmission::Timer> create() override
        {
            return std::make_unique<MockTimer>();
        }
    };

    class MockTimer final : public libtransmission::Timer
    {
        void stop() override
        {
        }

        void set_callback(std::function<void()> /* callback */) override
        {
        }

        void set_repeating(bool /* is_repeating */ = true) override
        {
        }

        void set_interval(std::chrono::milliseconds /* msec */) override
        {
        }

        void start() override
        {
        }

        [[nodiscard]] std::chrono::milliseconds interval() const noexcept override
        {
            return {};
        }

        [[nodiscard]] bool is_repeating() const noexcept override
        {
            return {};
        }
    };

    class MockMediator : public tr_global_ip_cache::Mediator
    {
    public:
        [[nodiscard]] libtransmission::TimerMaker& timer_maker() override
        {
            return timer_maker_;
        }

    private:
        MockTimerMaker timer_maker_;
    };

    void TearDown() override
    {
        if (global_ip_cache_)
        {
            global_ip_cache_->try_shutdown();
        }
        ::testing::Test::TearDown();
    }

    // To be created within the test body
    std::unique_ptr<tr_global_ip_cache> global_ip_cache_;
};

TEST_F(GlobalIPCacheTest, bindAddr)
{
    static constexpr auto AddrTests = std::array{
        std::array<std::pair<std::string_view, std::string_view>, 4>{ { { "8.8.8.8"sv, "8.8.8.8"sv },
                                                                        { "192.168.133.133"sv, "192.168.133.133"sv },
                                                                        { "2001:1890:1112:1::20"sv, "0.0.0.0"sv },
                                                                        { "asdasd"sv, "0.0.0.0"sv } } } /* IPv4 */,
        std::array<std::pair<std::string_view, std::string_view>, 4>{ { { "fd12:3456:789a:1::1"sv, "fd12:3456:789a:1::1"sv },
                                                                        { "192.168.133.133"sv, "::"sv },
                                                                        { "2001:1890:1112:1::20"sv, "2001:1890:1112:1::20"sv },
                                                                        { "asdasd"sv, "::"sv } } } /* IPv6 */
    };
    static_assert(TR_AF_INET == 0);
    static_assert(TR_AF_INET6 == 1);
    static_assert(NUM_TR_AF_INET_TYPES == 2);

    struct LocalMockMediator final : public MockMediator
    {
        [[nodiscard]] std::string_view settings_bind_addr(tr_address_type type) override
        {
            return AddrTests[type][j_].first;
        }

        std::size_t j_{};
    };

    auto mediator = LocalMockMediator{};
    global_ip_cache_ = tr_global_ip_cache::create(mediator);

    for (std::size_t i = 0; i < NUM_TR_AF_INET_TYPES; ++i)
    {
        mediator.j_ = 0;
        for (std::size_t& j = mediator.j_; j < std::size(AddrTests[i]); ++j)
        {
            auto const addr = global_ip_cache_->bind_addr(static_cast<tr_address_type>(i));
            EXPECT_EQ(addr.display_name(), AddrTests[i][j].second);
        }
    }
}

TEST_F(GlobalIPCacheTest, setGlobalAddr)
{
    static auto constexpr AddrStr = std::array{ "8.8.8.8"sv,
                                                "192.168.133.133"sv,
                                                "172.16.241.133"sv,
                                                "2001:1890:1112:1::20"sv,
                                                "fd12:3456:789a:1::1"sv };
    static auto constexpr AddrTests = std::array{ std::array{ true, false, false, false, false /* IPv4 */ },
                                                  std::array{ false, false, false, true, false /* IPv6 */ } };
    static_assert(TR_AF_INET == 0);
    static_assert(TR_AF_INET6 == 1);
    static_assert(NUM_TR_AF_INET_TYPES == 2);
    static_assert(std::size(AddrStr) == std::size(AddrTests[TR_AF_INET]));
    static_assert(std::size(AddrStr) == std::size(AddrTests[TR_AF_INET6]));

    auto mediator = MockMediator{};
    global_ip_cache_ = tr_global_ip_cache::create(mediator);

    for (std::size_t i = 0; i < NUM_TR_AF_INET_TYPES; ++i)
    {
        for (std::size_t j = 0; j < std::size(AddrStr); ++j)
        {
            auto const type = static_cast<tr_address_type>(i);
            auto const addr = tr_address::from_string(AddrStr[j]);
            ASSERT_TRUE(addr);
            EXPECT_EQ(global_ip_cache_->set_global_addr(type, *addr), AddrTests[i][j]);
            if (auto const val = global_ip_cache_->global_addr(type); val && AddrTests[i][j])
            {
                EXPECT_EQ(val->display_name(), AddrStr[j]);
            }
        }
    }
}

TEST_F(GlobalIPCacheTest, globalSourceIPv4)
{
    struct LocalMockMediator final : public MockMediator
    {
        [[nodiscard]] std::string_view settings_bind_addr(tr_address_type /* type */) override
        {
            return "0.0.0.0"sv;
        }
    };
    auto mediator = LocalMockMediator{};
    global_ip_cache_ = tr_global_ip_cache::create(mediator);

    global_ip_cache_->update_source_addr(TR_AF_INET);
    auto const addr = global_ip_cache_->global_source_addr(TR_AF_INET);
    if (!addr)
    {
        GTEST_SKIP() << "globalSourceIPv4 did not return an address, either:\n"
                     << "1. globalSourceIPv4 is broken\n"
                     << "2. Your system does not support IPv4\n"
                     << "3. You don't have IPv4 connectivity to public internet";
    }
    EXPECT_TRUE(addr->is_ipv4());
}

TEST_F(GlobalIPCacheTest, globalSourceIPv6)
{
    struct LocalMockMediator final : public MockMediator
    {
        [[nodiscard]] std::string_view settings_bind_addr(tr_address_type /* type */) override
        {
            return "::"sv;
        }
    };
    auto mediator = LocalMockMediator{};
    global_ip_cache_ = tr_global_ip_cache::create(mediator);

    global_ip_cache_->update_source_addr(TR_AF_INET6);
    auto const addr = global_ip_cache_->global_source_addr(TR_AF_INET6);
    if (!addr)
    {
        GTEST_SKIP() << "globalSourceIPv6 did not return an address, either:\n"
                     << "1. globalSourceIPv6 is broken\n"
                     << "2. Your system does not support IPv6\n"
                     << "3. You don't have IPv6 connectivity to public internet";
    }
    EXPECT_TRUE(addr->is_ipv6());
}

TEST_F(GlobalIPCacheTest, onResponseIPQuery)
{
    static auto constexpr AddrStr = std::array{
        "8.8.8.8"sv,      "192.168.133.133"sv,     "172.16.241.133"sv, "2001:1890:1112:1::20"sv, "fd12:3456:789a:1::1"sv,
        "91.121.74.28"sv, "2001:1890:1112:1::20"sv
    };
    static auto constexpr AddrTests = std::array{ std::array{ true, false, false, false, false, true, false /* IPv4 */ },
                                                  std::array{ false, false, false, true, false, false, true /* IPv6 */ } };
    static_assert(TR_AF_INET == 0);
    static_assert(TR_AF_INET6 == 1);
    static_assert(NUM_TR_AF_INET_TYPES == 2);
    static_assert(std::size(AddrStr) == std::size(AddrTests[TR_AF_INET]));
    static_assert(std::size(AddrStr) == std::size(AddrTests[TR_AF_INET6]));

    struct LocalMockMediator final : public MockMediator
    {
        void fetch(tr_web::FetchOptions&& options) override
        {
            auto response = tr_web::FetchResponse{ http_code,
                                                   std::string{ AddrStr[k_] },
                                                   true,
                                                   false,
                                                   options.done_func_user_data };
            options.done_func(response);
        }

        std::size_t address_type{};
        long http_code{};
        std::size_t k_{};
    };

    auto mediator = LocalMockMediator{};
    global_ip_cache_ = tr_global_ip_cache::create(mediator);

    mediator.address_type = 0;
    for (std::size_t& i = mediator.address_type; i < NUM_TR_AF_INET_TYPES; ++i)
    {
        // https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
        mediator.http_code = 100;
        for (long& j = mediator.http_code; j <= 599; ++j)
        {
            mediator.k_ = 0;
            for (std::size_t& k = mediator.k_; k < std::size(AddrStr); ++k)
            {
                auto const type = static_cast<tr_address_type>(i);

                global_ip_cache_->update_global_addr(type);

                auto const global_addr = global_ip_cache_->global_addr(type);
                EXPECT_EQ(!!global_addr, j == 200 /* HTTP_OK */ && AddrTests[i][k]);
                if (global_addr)
                {
                    EXPECT_EQ(global_addr->display_name(), AddrStr[k]);
                }
            }
        }
    }
}
