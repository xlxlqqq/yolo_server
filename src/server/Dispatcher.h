#pragma once

#include <string>

namespace server {

class Dispatcher {
public:
    static std::string dispatch(const std::string& request);
};  // class Dispatcher

};  // namespace server