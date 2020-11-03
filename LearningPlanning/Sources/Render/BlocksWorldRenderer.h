/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_FontCache.h"

#include "Render/DomainRenderer.h"
#include "Logic/Domain.h"

class BlocksWorldRenderer : public DomainRenderer {
public:
	void init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) override;
	~BlocksWorldRenderer();

	void renderState(State state, vector<Term> instances);

private:
	SDL_Texture* blockTexture;
	FC_Font* font;
};
