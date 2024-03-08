#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/mat2x2.hpp"
#include "glm/mat3x3.hpp"
#include "glm/mat4x4.hpp"

using mat2 = glm::mat2;
using mat3 = glm::mat3;
using mat4 = glm::mat4;

inline mat2 Inverse( const mat2& m ) { return glm::inverse( m ); }
inline mat3 Inverse( const mat3& m ) { return glm::inverse( m ); }
inline mat4 Inverse( const mat4& m ) { return glm::inverse( m ); }

inline mat2 Transpose( const mat2& m ) { return glm::transpose( m ); }
inline mat3 Transpose( const mat3& m ) { return glm::transpose( m ); }
inline mat4 Transpose( const mat4& m ) { return glm::transpose( m ); }