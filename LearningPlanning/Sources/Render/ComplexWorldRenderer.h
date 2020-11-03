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

class ComplexWorldRenderer : public DomainRenderer {
public:
	void init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) override;
	~ComplexWorldRenderer();

	void renderState(State state, vector<Term> instances);

private:
	void drawObject(State state, Term obj, SDL_Rect dst);

	SDL_Texture* blockTexture;
	SDL_Texture* pushableTexture;
	SDL_Texture* grabbableTexture;
	SDL_Texture* plugTexture;
	SDL_Texture* unpluggedTexture;
	SDL_Texture* lightTexture;
	SDL_Texture* paintTexture;
	SDL_Texture* robotTexture;

	FC_Font* font;
	FC_Font* smallFont;

	Term var;
	Predicate pred_delete;
	Predicate pred_is_paint;
	Predicate pred_has_color;
	Predicate pred_pushable;
	Predicate pred_grabbable;
	Predicate pred_at;
	Predicate pred_on;
	Predicate pred_on_floor;
	Predicate pred_lit;
	Predicate pred_plugged;
	Predicate pred_unplugged;
	Predicate pred_holding;
	Predicate pred_robot_at;
};
