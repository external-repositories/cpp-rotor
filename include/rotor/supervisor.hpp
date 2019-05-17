#pragma once

#include "actor.hpp"
#include "message.hpp"
#include "messages.hpp"
#include "system_context.hpp"
#include <boost/asio.hpp>
#include <deque>
#include <functional>
#include <iostream>
#include <unordered_map>

namespace rotor {

namespace asio = boost::asio;

template <typename T> struct handler_traits {};
template <typename A, typename M> struct handler_traits<void (A::*)(M &)> {
  using actor_t = A;
  using message_t = M;
};

struct supervisor_t : public actor_base_t {
public:
  supervisor_t(system_context_t &system_context_)
      : actor_base_t{*this},
        system_context{system_context_}, strand{system_context.io_context} {
    subscribe<payload::initialize_actor_t>(&actor_base_t::on_initialize);
  }

  template <typename M, typename... Args>
  void enqueue(const address_ptr_t &addr, Args &&... args) {
    auto raw_message = new message_t<M>(std::forward<Args>(args)...);
    outbound.emplace_back(addr, raw_message);
  }

  void start() {
    subscribe<payload::start_supervisor_t>(&supervisor_t::on_start);
    auto actor_ptr = actor_ptr_t(this);
    asio::defer(strand, [actor_ptr = std::move(actor_ptr)]() {
      auto &self = static_cast<supervisor_t &>(*actor_ptr);
      self.send<payload::start_supervisor_t>(self.address);
      self.dequeue();
    });
  }

  void on_start(message_t<payload::start_supervisor_t> &) {
    std::cout << "on_start\n";
  }

  inline system_context_t &get_context() { return system_context; }

  template <typename M, typename A, typename Handler>
  void subscribe_actor(A &actor, Handler &&handler) {
    using traits = handler_traits<Handler>;
    using final_messaget_t = typename traits::message_t;
    using final_actor_t = typename traits::actor_t;

    auto actor_ptr = actor_ptr_t(&actor);
    auto address = actor.get_address();
    handler_t fn = [actor_ptr = std::move(actor_ptr),
                    handler = std::move(handler)](message_ptr_t &msg) mutable {
      auto final_message = dynamic_cast<final_messaget_t *>(msg.get());
      // final_message.payload;
      if (final_message) {
        auto &final_obj = static_cast<final_actor_t &>(*actor_ptr);
        (final_obj.*handler)(*final_message);
      }
    };
    handler_map.emplace(std::move(address), std::move(fn));
  }

private:
  void dequeue() {
    while (outbound.size()) {
      auto &item = outbound.front();
      auto range = handler_map.equal_range(item.address);
      for (auto it = range.first; it != range.second; ++it) {
        auto &handler = it->second;
        handler(item.message);
      }
      outbound.pop_front();
    }
  }

  struct item_t {
    address_ptr_t address;
    message_ptr_t message;

    item_t(const address_ptr_t &address_, message_ptr_t message_)
        : address{address_}, message{message_} {}
  };
  using queue_t = std::deque<item_t>;
  using handler_t = std::function<void(message_ptr_t &)>;
  using handler_map_t = std::unordered_multimap<address_ptr_t, handler_t>;

  system_context_t &system_context;
  asio::io_context::strand strand;
  queue_t outbound;
  handler_map_t handler_map;
};

inline system_context_t &get_context(supervisor_t &supervisor) {
  return supervisor.get_context();
}

using supervisor_ptr_t = boost::intrusive_ptr<supervisor_t>;

/* third-party classes implementations */

template <typename Supervisor, typename... Args>
auto system_context_t::create_supervisor(Args... args)
    -> boost::intrusive_ptr<Supervisor> {
  using wrapper_t = boost::intrusive_ptr<Supervisor>;
  auto raw_object = new Supervisor{*this, std::forward<Args>(args)...};
  supervisor = supervisor_ptr_t{raw_object};
  supervisor->send<payload::initialize_actor_t>(supervisor->get_address());
  return wrapper_t{raw_object};
}

template <typename M, typename... Args>
void actor_base_t::send(const address_ptr_t &addr, Args &&... args) {
  supervisor.enqueue<M>(addr, std::forward<Args>(args)...);
}

template <typename M, typename Handler>
void actor_base_t::subscribe(Handler &&h) {
  supervisor.subscribe_actor<M>(*this, std::forward<Handler>(h));
}

} // namespace rotor