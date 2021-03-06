//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/behavior.h"
#include "cassert"

using namespace rotor;

actor_behavior_t::~actor_behavior_t() {}

/* init related */
void actor_behavior_t::on_start_init() noexcept {
    assert(actor.state == state_t::INITIALIZING);
    substate = behavior_state_t::INIT_STARTED;
    action_confirm_init();
}

void actor_behavior_t::action_confirm_init() noexcept {
    if (actor.init_request) {
        actor.reply_to(*actor.init_request);
        actor.init_request.reset();
    }
    actor.state = state_t::INITIALIZED;
    return action_finish_init();
}

void actor_behavior_t::action_finish_init() noexcept {
    actor.init_finish();
    substate = behavior_state_t::INIT_ENDED;
}

/* shutdown related */

void actor_behavior_t::on_start_shutdown() noexcept {
    substate = behavior_state_t::SHUTDOWN_STARTED;
    actor.state = state_t::SHUTTING_DOWN;
    action_unsubscribe_self();
}

void actor_behavior_t::action_unsubscribe_self() noexcept {
    substate = behavior_state_t::UNSUBSCRIPTION_STARTED;
    actor_ptr_t self{&actor};
    actor.get_supervisor().unsubscribe_actor(self);
}

void actor_behavior_t::action_confirm_shutdown() noexcept {
    if (actor.shutdown_request) {
        actor.reply_to(*actor.shutdown_request);
        actor.shutdown_request.reset();
    }
    return action_commit_shutdown();
}

void actor_behavior_t::action_commit_shutdown() noexcept {
    assert(actor.state == state_t::SHUTTING_DOWN);
    actor.state = state_t::SHUTTED_DOWN;
    return action_finish_shutdown();
}

void actor_behavior_t::action_finish_shutdown() noexcept {
    actor.shutdown_finish();
    substate = behavior_state_t::SHUTDOWN_ENDED;
}

void actor_behavior_t::on_unsubscription() noexcept {
    assert(substate == behavior_state_t::UNSUBSCRIPTION_STARTED);
    action_confirm_shutdown();
}

supervisor_behavior_t::supervisor_behavior_t(supervisor_t &sup) : actor_behavior_t{sup} {}

void supervisor_behavior_t::on_start_shutdown() noexcept {
    substate = behavior_state_t::SHUTDOWN_STARTED;
    auto &sup = static_cast<supervisor_t &>(actor);
    sup.state = state_t::SHUTTING_DOWN;
    action_shutdown_children();
}

void supervisor_behavior_t::action_shutdown_children() noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    auto &actors_map = sup.actors_map;
    if (!actors_map.empty()) {
        for (auto &pair : actors_map) {
            auto &state = pair.second;
            if (!state.shutdown_requesting) {
                auto &addr = pair.first;
                state.shutdown_requesting = true;
                sup.request<payload::shutdown_request_t>(addr, addr).send(sup.shutdown_timeout);
            }
        }
        substate = behavior_state_t::SHUTDOWN_CHILDREN_STARTED;
    } else {
        return action_unsubscribe_self();
    }
}

void supervisor_behavior_t::on_childen_removed() noexcept {
    assert(substate == behavior_state_t::SHUTDOWN_CHILDREN_STARTED);
    action_unsubscribe_self();
}

void supervisor_behavior_t::on_shutdown_fail(const address_ptr_t &, const std::error_code &ec) noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    sup.context->on_error(ec);
}

void supervisor_behavior_t::on_start_init() noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    (void)sup;
    assert(sup.state == state_t::INITIALIZING);
    substate = behavior_state_t::INIT_STARTED;
    if (initializing_actors.empty()) {
        action_confirm_init();
    }
}

void supervisor_behavior_t::on_create_child(const address_ptr_t &address) noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    if (sup.state == state_t::INITIALIZING) {
        initializing_actors.emplace(address);
    }
}

void supervisor_behavior_t::on_init(const address_ptr_t &address, const std::error_code &ec) noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    bool continue_init = false;
    bool in_init = sup.state == state_t::INITIALIZING;
    auto it = initializing_actors.find(address);
    if (it != initializing_actors.end()) {
        initializing_actors.erase(it);
        continue_init = in_init;
    }
    if (ec) {
        auto shutdown_self = in_init && sup.policy == supervisor_policy_t::shutdown_self;
        if (shutdown_self) {
            continue_init = false;
            sup.do_shutdown();
        } else {
            sup.template request<payload::shutdown_request_t>(address, address).send(sup.shutdown_timeout);
        }
    } else {
        sup.template send<payload::start_actor_t>(address, address);
    }
    if (continue_init) {
        on_start_init();
    }
}
