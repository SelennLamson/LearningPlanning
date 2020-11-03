/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <vector>
#include "SDL.h"

#include "Logic/Domain.h"

using namespace std;

class DomainRenderer {
public:
	DomainRenderer() {}

	virtual void init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) {
		domain = inDomain;
		renderer = inRenderer;
		window = inWindow;
	}

	virtual void renderState(State state, vector<Term> instances) {}

protected:
	shared_ptr<Domain> domain = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_Window* window = nullptr;
};
