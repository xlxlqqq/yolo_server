#pragma once

#include "storage/YoloStorage.h"
#include "storage/YoloTypes.h"

#include <string>

namespace storage {

std::string encode(const YoloFrame& frame);
YoloFrame decode(const std::string& value);

}
