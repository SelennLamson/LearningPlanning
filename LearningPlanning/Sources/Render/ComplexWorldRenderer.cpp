/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Render/ComplexWorldRenderer.h"

#include <iostream>
#include <vector>
#include <string>

#define MARGIN 20

using namespace std;

void ComplexWorldRenderer::init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) {
	DomainRenderer::init(inDomain, inRenderer, inWindow);

	SDL_Surface* tmpSurface = IMG_Load("resources/block.png");
	blockTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/grabbable.png");
	grabbableTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/pushable.png");
	pushableTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/plug.png");
	plugTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/unplugged.png");
	unpluggedTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/light.png");
	lightTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/paint.png");
	paintTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/robot.png");
	robotTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	SDL_FreeSurface(tmpSurface);

	font = FC_CreateFont();
	FC_LoadFont(font, renderer, "resources/consola.ttf", 20, FC_MakeColor(0, 0, 0, 255), TTF_STYLE_NORMAL);
	
	smallFont = FC_CreateFont();
	FC_LoadFont(smallFont, renderer, "resources/consola.ttf", 12, FC_MakeColor(0, 0, 0, 255), TTF_STYLE_NORMAL);

	var = Variable("X");
	pred_delete = domain->getPredByName("delete");
	pred_is_paint = domain->getPredByName("is-paint");
	pred_has_color = domain->getPredByName("has-color");
	pred_pushable = domain->getPredByName("pushable");
	pred_grabbable = domain->getPredByName("grabbable");
	pred_at = domain->getPredByName("at");
	pred_on = domain->getPredByName("on");
	pred_on_floor = domain->getPredByName("on-floor");
	pred_lit = domain->getPredByName("lit");
	pred_plugged = domain->getPredByName("plugged");
	pred_unplugged = domain->getPredByName("unplugged");
	pred_holding = domain->getPredByName("holding");
	pred_robot_at = domain->getPredByName("robot-at");
}

ComplexWorldRenderer::~ComplexWorldRenderer() {
	SDL_DestroyTexture(blockTexture);
	SDL_DestroyTexture(pushableTexture);
	SDL_DestroyTexture(grabbableTexture);
	SDL_DestroyTexture(paintTexture);
	SDL_DestroyTexture(plugTexture);
	SDL_DestroyTexture(unpluggedTexture);
	SDL_DestroyTexture(lightTexture);
	SDL_DestroyTexture(robotTexture);
	FC_FreeFont(font);
	FC_FreeFont(smallFont);
}

void ComplexWorldRenderer::renderState(State state, vector<Term> instances) {
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

	const int blockSize = 32;
	SDL_Rect dst;
	dst.w = blockSize;
	dst.h = blockSize;

	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	vector<Term> locations = filterDeleted(filterByType(instances, domain->getTypeByName("location")), state, pred_delete);

	vector<Term> onFloorObjs;
	set<Literal> query = state.query(Literal(pred_on_floor, { var }));
	foreach(q, query)
		onFloorObjs.push_back(q->parameters[0]);

	int x = MARGIN;
	int objsInLoc;

	foreachindex(i, locations) {
		objsInLoc = 0;

		Term loc = locations[i];
		SDL_Rect locRect;
		locRect.x = x;
		locRect.y = h - MARGIN;
		locRect.w = MARGIN;
		locRect.h = h - 2 * MARGIN;

		dst.x = locRect.x + MARGIN;
		foreachindex(j, onFloorObjs) {
			Term obj = onFloorObjs[j];

			if (!state.query(Literal(pred_at, { obj, loc })).empty()) {
				objsInLoc++;
				dst.y = locRect.y - MARGIN - dst.h;
				drawObject(state, obj, dst);
				dst.x += MARGIN + dst.w;
			}
		}

		locRect.w += objsInLoc * (MARGIN + dst.w);
		locRect.w = max(locRect.w, 6 * MARGIN + 4 * dst.w);

		if (!state.query(Literal(pred_robot_at, { loc })).empty()) {
			dst.y = locRect.y - locRect.h + MARGIN;
			dst.x = locRect.x + locRect.w - 2 *(MARGIN + dst.w);
			SDL_RenderCopy(renderer, robotTexture, NULL, &dst);

			dst.x += MARGIN + dst.w;
			query = state.query(Literal(pred_holding, { var }));
			if (!query.empty())
				drawObject(state, query.begin()->parameters[0], dst);
		}

		SDL_RenderDrawLine(renderer, locRect.x, locRect.y, locRect.x + locRect.w, locRect.y);
		SDL_RenderDrawLine(renderer, locRect.x + locRect.w , locRect.y, locRect.x + locRect.w, locRect.y - locRect.h);
		SDL_RenderDrawLine(renderer, locRect.x + locRect.w, locRect.y - locRect.h, locRect.x, locRect.y - locRect.h);
		SDL_RenderDrawLine(renderer, locRect.x, locRect.y - locRect.h, locRect.x, locRect.y);
		FC_Draw(font, renderer, 1.0f * locRect.x + MARGIN, 1.0f * locRect.y - locRect.h + MARGIN, loc.name.c_str());

		x += locRect.w + MARGIN;
	}

	SDL_RenderPresent(renderer);
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
}

void ComplexWorldRenderer::drawObject(State state, Term obj, SDL_Rect dst) {
	set<Literal> query = state.query(Literal(pred_has_color, { obj, var }));
	if (!query.empty()) {
		Term color = query.begin()->parameters[1];
		tuple<Uint8, Uint8, Uint8> values = in(colorMap, color.name) ? colorMap[color.name] : tuple<Uint8, Uint8, Uint8>{ 255, 255, 255 };
		SDL_SetTextureColorMod(blockTexture, get<0>(values), get<1>(values), get<2>(values));
	}
	else {
		SDL_SetTextureColorMod(blockTexture, 255, 255, 255);
	}
	SDL_RenderCopy(renderer, blockTexture, NULL, &dst);

	if (!state.query(Literal(pred_pushable, { obj })).empty())
		SDL_RenderCopy(renderer, pushableTexture, NULL, &dst);

	if (!state.query(Literal(pred_grabbable, { obj })).empty())
		SDL_RenderCopy(renderer, grabbableTexture, NULL, &dst);

	if (!state.query(Literal(pred_lit, { obj })).empty())
		SDL_RenderCopy(renderer, lightTexture, NULL, &dst);

	if (!state.query(Literal(pred_plugged, { obj, var })).empty())
		SDL_RenderCopy(renderer, plugTexture, NULL, &dst);

	if (!state.query(Literal(pred_unplugged, { obj })).empty())
		SDL_RenderCopy(renderer, unpluggedTexture, NULL, &dst);

	query = state.query(Literal(pred_is_paint, { obj, var }));
	if (!query.empty()) {
		Term color = query.begin()->parameters[1];
		tuple<Uint8, Uint8, Uint8> values = in(colorMap, color.name) ? colorMap[color.name] : tuple<Uint8, Uint8, Uint8>{ 255, 255, 255 };
		SDL_SetTextureColorMod(paintTexture, get<0>(values), get<1>(values), get<2>(values));
		SDL_RenderCopy(renderer, paintTexture, NULL, &dst);
	}

	FC_Draw(smallFont, renderer, 1.0f * dst.x, 1.0f * dst.y + dst.h, obj.name.c_str());

	query = state.query(Literal(pred_on, { var, obj }));
	if (!query.empty()) {
		dst.y -= dst.h + MARGIN;
		drawObject(state, query.begin()->parameters[0], dst);
	}
}
