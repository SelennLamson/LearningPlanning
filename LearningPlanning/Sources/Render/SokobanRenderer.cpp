/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Render/SokobanRenderer.h"

#include <iostream>
#include <vector>
#include <string>

#define MARGIN 20

using namespace std;

void SokobanRenderer::init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) {
	DomainRenderer::init(inDomain, inRenderer, inWindow);

	SDL_Surface* tmpSurface = IMG_Load("resources/block.png");
	blockTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/cell.png");
	cellTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/robot.png");
	playerTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	SDL_FreeSurface(tmpSurface);

	var = Variable("X");
	blockVar = Variable("B", domain->getTypeByName("crate"));
	playerVar = Variable("P", domain->getTypeByName("player"));
	pred_up = domain->getPredByName("up");
	pred_right = domain->getPredByName("right");
	pred_at = domain->getPredByName("at");
	pred_clear = domain->getPredByName("clear");
	pred_delete = domain->getPredByName("delete");
}

SokobanRenderer::~SokobanRenderer() {
	SDL_DestroyTexture(blockTexture);
	SDL_DestroyTexture(cellTexture);
	SDL_DestroyTexture(playerTexture);
}

void SokobanRenderer::renderState(State state, vector<Term> instances) {
	SDL_RenderClear(renderer);

	const int cellSize = 32;
	SDL_Rect dst;
	dst.w = cellSize;
	dst.h = cellSize;

	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	vector<Term> cells = filterDeleted(filterByType(instances, domain->getTypeByName("location")), state, pred_delete);

	foreach(cell, cells) {
		string name = cell->name;
		string tmp;
		char c = ' ';
		int field = 0;
		int x = 0;
		int y = 0;
		for (unsigned int i = 0; i < name.length() + 1; i++) {
			if (i < name.length())
				c = name[i];

			if (i == name.length() || c == '_') {
				if (field == 1)
					x = atoi(tmp.c_str());
				else if (field == 2)
					y = atoi(tmp.c_str());
				tmp = "";
				field++;
			}
			else
				tmp += c;
		}

		dst.x = MARGIN + x * dst.w;
		dst.y = MARGIN + y * dst.h;
		SDL_RenderCopy(renderer, cellTexture, NULL, &dst);

		if (!state.query(Literal(pred_at, { playerVar, *cell })).empty())
			SDL_RenderCopy(renderer, playerTexture, NULL, &dst);

		if (!state.query(Literal(pred_at, { blockVar, *cell })).empty())
			SDL_RenderCopy(renderer, blockTexture, NULL, &dst);
	}

	SDL_RenderPresent(renderer);
}
