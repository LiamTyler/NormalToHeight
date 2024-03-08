#pragma once

#include <chrono>

namespace PG::Time
{

using Point = std::chrono::high_resolution_clock::time_point;

Point GetTimePoint();

// Returns the number of milliseconds elapsed since Point
double GetTimeSince( const Point& point );

// Returns the number of milliseconds between two points
double GetElapsedTime( const Point& startPoint, const Point& endPoint );

} // namespace PG::Time
