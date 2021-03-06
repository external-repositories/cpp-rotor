//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "rotor/asio.hpp"
#include "supervisor_asio_test.h"

#include <iostream>

namespace r = rotor;
namespace ra = rotor::asio;
namespace rt = r::test;
namespace asio = boost::asio;
namespace pt = boost::posix_time;

struct ping_t {};
struct pong_t {};

static std::uint32_t destroyed = 0;

struct pinger_t : public r::actor_base_t {
    std::uint32_t ping_sent;
    std::uint32_t pong_received;
    rotor::address_ptr_t ponger_addr;

    explicit pinger_t(rotor::supervisor_t &sup) : r::actor_base_t{sup} { ping_sent = pong_received = 0; }
    ~pinger_t() { destroyed += 1; }

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void init_start() noexcept override {
        subscribe(&pinger_t::on_pong);
        r::actor_base_t::init_start();
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        send<ping_t>(ponger_addr);
        ++ping_sent;
    }

    void on_pong(rotor::message_t<pong_t> &) noexcept {
        ++pong_received;
        supervisor.shutdown();
    }
};

struct ponger_t : public r::actor_base_t {
    std::uint32_t pong_sent;
    std::uint32_t ping_received;
    rotor::address_ptr_t pinger_addr;

    explicit ponger_t(rotor::supervisor_t &sup) : rotor::actor_base_t{sup} { pong_sent = ping_received = 0; }
    ~ponger_t() { destroyed += 3; }

    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void init_start() noexcept override {
        subscribe(&ponger_t::on_ping);
        r::actor_base_t::init_start();
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept {
        ++ping_received;
        send<pong_t>(pinger_addr);
        ++pong_sent;
    }
};

TEST_CASE("ping/pong ", "[supervisor][asio]") {
    asio::io_context io_context{1};
    auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
    auto stand = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = r::pt::milliseconds{10};
    ra::supervisor_config_asio_t conf{timeout, std::move(stand)};
    auto sup = system_context->create_supervisor<rt::supervisor_asio_test_t>(conf);

    auto pinger = sup->create_actor<pinger_t>(timeout);
    auto ponger = sup->create_actor<ponger_t>(timeout);
    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup->start();
    io_context.run();

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->pong_sent == 1);
    REQUIRE(ponger->ping_received == 1);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);

    pinger.reset();
    ponger.reset();
    REQUIRE(sup->get_timers_map().size() == 0);
    REQUIRE(destroyed == 4);
}
