// my_logger.h
#pragma once

#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

namespace my_logger {

using namespace boost::json;
namespace logging = boost::log;
    
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", value)

void MyFormatter(
    logging::record_view const& rec,
    logging::formatting_ostream& strm);

void InitBoostLogFilter();

} // namespace my_logger