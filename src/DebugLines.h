#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "RenderContext.h"

void initDebugLines();
void deinitDebugLines();

void clearDebugLines();
void drawDebugLine(const glm::vec3 &a, const glm::vec3 &b, const glm::vec4 &color);

void updateDebugLines(RenderContext &context);
void renderDebugLines(RenderContext &context);
