#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <boost/date_time/posix_time/posix_time.hpp>

namespace rotor {

namespace pt = boost::posix_time;

/** \struct supervisor_config_t
 *  \brief base supervisor config, which holds shutdowm timeout value
 */
struct supervisor_config_t {
    /** \brief how much time is allowed to spend in shutdown for children actor */
    pt::time_duration shutdown_timeout;
};

} // namespace rotor