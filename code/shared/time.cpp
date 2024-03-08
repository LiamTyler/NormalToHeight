#include "time.hpp"

using namespace std::chrono;
using Clock     = high_resolution_clock;
using TimePoint = time_point<Clock>;

namespace PG::Time
{

Point GetTimePoint() { return Clock::now(); }

double GetTimeSince( const Point& point )
{
    auto now = Clock::now();
    return duration_cast<microseconds>( now - point ).count() / 1000.0;
}

double GetElapsedTime( const Point& startPoint, const Point& endPoint )
{
    return duration_cast<microseconds>( endPoint - startPoint ).count() / 1000.0;
}

} // namespace PG::Time
