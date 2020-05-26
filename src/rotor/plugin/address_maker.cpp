//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/address_maker.h"
#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
#include <typeinfo>

using namespace rotor;
using namespace rotor::internal;

const void* address_maker_plugin_t::class_identity = static_cast<const void *>(typeid(address_maker_plugin_t).name());

const void* address_maker_plugin_t::identity() const noexcept {
    return class_identity;
}

void address_maker_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;

    if (!actor_->address) {
        actor_->address = create_address();
    }
    plugin_t::activate(actor_);
    actor->init_start();
}


void address_maker_plugin_t::deactivate() noexcept {
    return plugin_t::deactivate();
}

address_ptr_t address_maker_plugin_t::create_address() noexcept {
    return actor->get_supervisor().make_address();
}