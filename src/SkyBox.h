#pragma once

#include "RenderContext.h"

void initSkyBox();
void deinitSkyBox();

void setSkyBoxTexture(const std::string &path);

void renderSkyBox(RenderContext &context);
