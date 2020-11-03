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

class SokobanRenderer : public DomainRenderer {
public:
	void init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) override;
	~SokobanRenderer();

	void renderState(State state, vector<Term> instances);

private:
	SDL_Texture* blockTexture;
	SDL_Texture* playerTexture;
	SDL_Texture* cellTexture;

	Term var;
	Term playerVar;
	Term blockVar;
	Predicate pred_up;
	Predicate pred_right;
	Predicate pred_at;
	Predicate pred_clear;
	Predicate pred_delete;
};
