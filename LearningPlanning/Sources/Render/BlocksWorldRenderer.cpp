/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Render/BlocksWorldRenderer.h"

#include <iostream>
#include <vector>
#include <string>

using namespace std;

void BlocksWorldRenderer::init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) {
	DomainRenderer::init(inDomain, inRenderer, inWindow);

	SDL_Surface* tmpSurface = IMG_Load("resources/block.png");
	blockTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);
	SDL_FreeSurface(tmpSurface);

	font = FC_CreateFont();
	FC_LoadFont(font, renderer, "resources/consola.ttf", 20, FC_MakeColor(0, 0, 0, 255), TTF_STYLE_NORMAL);
}

BlocksWorldRenderer::~BlocksWorldRenderer() {
	SDL_DestroyTexture(blockTexture);
	FC_FreeFont(font);
}

void BlocksWorldRenderer::renderState(State state, vector<Term> instances) {
	SDL_RenderClear(renderer);

	int blockSize = 32;
	SDL_Rect dst;
	dst.w = blockSize;
	dst.h = blockSize;

	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	Variable var = Variable("X", domain->getTypeByName("object"));
	Predicate pred_on = domain->getPredByName("on");
	Predicate pred_holding = domain->getPredByName("holding");
	Predicate pred_ontable = domain->getPredByName("on-table");
	
	// Optional (for colorblocksworld)
	Predicate pred_w = domain->getPredByName("w");
	Predicate pred_b = domain->getPredByName("b");
	Term aFloor;
	bool colors = pred_w != Predicate();
	if (colors)
		aFloor = domain->getConstantByName("floor").obj;

	vector<vector<Term>> blocks;

	set<Literal> onTableBlocks;
	if (colors)
		onTableBlocks = state.query(Literal(pred_on, { var, aFloor }));
	else
		onTableBlocks = state.query(Literal(pred_ontable, { var }));

	int total = 0;
	foreach(it, onTableBlocks) {
		total++;
		vector<Term> tower;

		Literal currentFact = (*it);
		bool found = true;
		while (found) {
			Term currentBlock = currentFact.parameters[0];
			tower.push_back(currentBlock);

			set<Literal> newOn = state.query(Literal(pred_on, { var, currentBlock }));
			if (newOn.size() > 0) {
				currentFact = *(newOn.begin());
				total++;
			}
			else {
				found = false;
			}
		}

		blocks.push_back(tower);
	}

	for (int i = 0; i < blocks.size(); i++) {
		dst.x = (5 + blockSize) * i + 5;

		for (int j = 0; j < blocks[i].size(); j++) {
			dst.y = h - (5 + blockSize - 12) * j - 5 - blockSize;

			if (colors) {
				set<Literal> query = state.query(Literal(pred_w, { blocks[i][j] }));
				if (!query.empty()) {
					SDL_SetTextureColorMod(blockTexture, 255, 255, 255);
				}
				else {
					SDL_SetTextureColorMod(blockTexture, 20, 20, 20);
				}
			}

			SDL_RenderCopy(renderer, blockTexture, NULL, &dst);
			FC_Draw(font, renderer, dst.x + 5.0f, dst.y + 12.0f, blocks[i][j].name.c_str());
		}
	}

	if (!colors) {
		set<Literal> held = state.query(Literal(pred_holding, { var }));
		if (held.size() > 0) {
			total++;
			dst.x = 5;
			dst.y = 5;

			SDL_RenderCopy(renderer, blockTexture, NULL, &dst);
			FC_Draw(font, renderer, dst.x + 5.0f, dst.y + 12.0f, (*held.begin()).parameters[0].name.c_str());
		}
	}

	SDL_RenderPresent(renderer);
}
