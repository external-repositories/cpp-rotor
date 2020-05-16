#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"
#include <unordered_set>

namespace rotor::internal {

struct child_manager_plugin_t: public plugin_t {
    using plugin_t::plugin_t;


    /** \brief child actror housekeeping structure */
    struct actor_state_t {
        /** \brief intrusive pointer to actor */
        actor_ptr_t actor;

        /** \brief whethe the shutdown request is already sent */
        bool shutdown_requesting;
    };

    /** \brief (local) address-to-child_actor map type */
    using actors_map_t = std::unordered_map<address_ptr_t, actor_state_t>;

    /** \brief type for keeping list of initializing actors (during supervisor inititalization) */
    using initializing_actors_t = std::unordered_set<address_ptr_t>;

    static const void* class_identity;
    const void* identity() const noexcept override;

    bool activate(actor_base_t* actor) noexcept override;
    bool deactivate() noexcept override;

    virtual void create_child(const actor_ptr_t &actor) noexcept;
    virtual void remove_child(actor_base_t &actor) noexcept;
    virtual void on_shutdown_fail(actor_base_t &actor, const std::error_code &ec) noexcept;

    virtual void on_create(message::create_actor_t& message) noexcept;
    virtual void on_init(message::init_response_t& message) noexcept;
    virtual void on_shutdown_trigger(message::shutdown_trigger_t& message) noexcept;
    virtual void on_shutdown_confirm(message::shutdown_response_t& message) noexcept;

    virtual bool handle_init(message::init_request_t*) noexcept override;
    virtual bool handle_shutdown(message::shutdown_request_t*) noexcept override;

    void unsubscribe_all() noexcept;

    bool postponed_init = false;
    bool activated = false;
    /** \brief local address to local actor (intrusive pointer) mapping */
    actors_map_t actors_map;

    /** \brief list of initializing actors (during supervisor inititalization) */
    initializing_actors_t initializing_actors;
};

}