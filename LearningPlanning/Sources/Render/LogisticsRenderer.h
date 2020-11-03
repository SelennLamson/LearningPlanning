/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * Icons made by Freepik from flaticon.com
 * https://www.flaticon.com/authors/freepik
 * https://www.flaticon.com/packs/delivery-180?k=1594301885684
 */

#pragma once

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_FontCache.h"

#include "Render/DomainRenderer.h"
#include "Logic/Domain.h"

class LogisticsRenderer : public DomainRenderer {
public:
	void init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) override;
	~LogisticsRenderer();

	void renderState(State state, vector<Term> instances);

private:
	SDL_Texture* boxTexture = nullptr;
	SDL_Texture* cityTexture = nullptr;
	SDL_Texture* truckTexture = nullptr;
	FC_Font* font = nullptr;

	shared_ptr<TermType> tBox, tTruck, tCity;
	Variable vBox, vTruck;
	Predicate pBoxin, pTruckin;
};
