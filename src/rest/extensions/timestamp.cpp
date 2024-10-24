/*
 *  Copyright (c) 2024, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "timestamp.hpp"

#include <chrono>
#include <ctime>
//#include <iomanip>
//#include <sstream>
#include <cstring>

using namespace std;
using namespace std::chrono;

std::string now_rfc3339()
{
    return toRfc3339(system_clock::now());
}

/*
// create UTC timestamp
std::string toRfc3339Utc(system_clock::time_point timepoint)
{
    const auto now_ms = time_point_cast<milliseconds>(timepoint);
    const auto now_s = time_point_cast<seconds>(now_ms);
    const auto millis = now_ms - now_s;
    const auto c_now = system_clock::to_time_t(now_s);

    std::stringstream ss;
    ss << put_time(gmtime(&c_now), "%FT%T")
       << '.' << setfill('0') << setw(3) << millis.count() << 'Z';
    return ss.str();
}
*/

std::string toRfc3339(system_clock::time_point timepoint)
{
    char        updated_str[26] = {'\0'};
    std::time_t time_since_epoch;
    std::time_t utc_offset;
    char        offset_str[6] = {'\0'};
    char        sign[2]       = {'\0'};

    time_since_epoch = std::chrono::system_clock::to_time_t(timepoint);
    utc_offset =
        std::difftime(std::mktime(std::localtime(&time_since_epoch)), std::mktime(std::gmtime(&time_since_epoch)));

    std::strftime(offset_str, sizeof(offset_str), "%H:%M", std::localtime(&utc_offset));
    strftime(updated_str, sizeof(updated_str), "%Y-%m-%dT%H:%M:%S", std::localtime(&time_since_epoch));

    sign[0] = utc_offset < 0 ? '-' : '+';
    sign[1] = '\0';

    strcat(updated_str, sign);
    strcat(updated_str, offset_str);

    return std::string(updated_str);
}
