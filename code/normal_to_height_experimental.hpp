#pragma once

#include "normal_to_height.hpp"

GenerationResults GetHeightMapFromNormalMap_WithEdges( const FloatImage2D& normalMap, uint32_t iterations, float iterationMultiplier = 1.0f );

GenerationResults GetHeightMapFromNormalMap_LinearSolve( const FloatImage2D& normalMap, uint32_t iterations, bool linearSolveWithGuess = true );
