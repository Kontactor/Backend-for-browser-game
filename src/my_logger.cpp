// my_logger.cpp
#include "my_logger.h"

#include <boost/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include <iostream>
#include <string_view>

namespace my_logger {

using namespace boost::json;
using namespace std::literals;

namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void MyFormatter(
    logging::record_view const& rec,
    logging::formatting_ostream& strm)
{
    auto ts = *rec[timestamp];
    value message{
        {"Timestamp", to_iso_extended_string(ts)},
        {"data", *rec[additional_data]},
        {"message", *rec[expr::smessage]}
    };
    strm << message;
}

void InitBoostLogFilter() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout,
        keywords::format = &MyFormatter,
        keywords::auto_flush = true
    );
}

} // namespace my_logger