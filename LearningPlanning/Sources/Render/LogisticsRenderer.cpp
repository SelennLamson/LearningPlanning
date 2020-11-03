/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * Icons made by Freepik from flaticon.com
 * https://www.flaticon.com/authors/freepik
 * https://www.flaticon.com/packs/delivery-180?k=1594301885684
 */

#include "Render/LogisticsRenderer.h"

#include <iostream>
#include <vector>
#include <string>

using namespace std;

void LogisticsRenderer::init(shared_ptr<Domain> inDomain, SDL_Renderer* inRenderer, SDL_Window* inWindow) {
	DomainRenderer::init(inDomain, inRenderer, inWindow);

	SDL_Surface* tmpSurface = IMG_Load("resources/box.png");
	boxTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/warehouse.png");
	cityTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	tmpSurface = IMG_Load("resources/truck.png");
	truckTexture = SDL_CreateTextureFromSurface(renderer, tmpSurface);

	SDL_FreeSurface(tmpSurface);

	font = FC_CreateFont();
	FC_LoadFont(font, renderer, "resources/consola.ttf", 20, FC_MakeColor(0, 0, 0, 255), TTF_STYLE_NORMAL);

	tBox = domain->getTypeByName("box");
	tTruck = domain->getTypeByName("truck");
	tCity = domain->getTypeByName("city");

	vBox = Variable("B", tBox);
	vTruck = Variable("T", tTruck);

	pBoxin = domain->getPredByName("boxin");
	pTruckin = domain->getPredByName("truckin");
}

LogisticsRenderer::~LogisticsRenderer() {
	SDL_DestroyTexture(boxTexture);
	SDL_DestroyTexture(cityTexture);
	SDL_DestroyTexture(truckTexture);
	FC_FreeFont(font);
}

void LogisticsRenderer::renderState(State state, vector<Term> instances) {
	SDL_RenderClear(renderer);

	int elemSize = 64;
	SDL_Rect dst;
	dst.w = elemSize;
	dst.h = elemSize;
	int margin = 4;

	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	vector<Term> boxes = filterByType(instances, tBox);
	vector<Term> trucks = filterByType(instances, tTruck);
	vector<Term> cities = filterByType(instances, tCity);

	int currentWidth = margin;
	foreach(cit, cities) {
		set<Literal> boxesInCity = state.query(Literal(pBoxin, { vBox, *cit }));
		set<Literal> trucksInCity = state.query(Literal(pTruckin, { vTruck, *cit }));

		size_t nBoxes = boxesInCity.size();
		size_t nTrucks = trucksInCity.size();
		int totalWidth = max(3 * (elemSize + margin), (elemSize + margin) * max((int)nBoxes, (int)nTrucks)) - margin;
		int center = totalWidth / 2;

		int shiftBoxes = (totalWidth - (elemSize + margin) * (int)nBoxes) / 2;
		int shiftTrucks = (totalWidth - (elemSize + margin) * (int)nTrucks) / 2;

		// Printing boxes in city
		dst.y = h - 1 * (margin + elemSize);
		int i = 0;
		foreach(bit, boxesInCity) {
			dst.x = currentWidth + i * (elemSize + margin) + shiftBoxes;

			SDL_RenderCopy(renderer, boxTexture, NULL, &dst);
			FC_Draw(font, renderer, dst.x - 15.0f, dst.y * 1.0f, bit->parameters[0].name.c_str());
			i++;
		}

		// Printing city
		dst.y = h - 2 * (margin + elemSize);
		dst.x = currentWidth + center - elemSize / 2;
		SDL_RenderCopy(renderer, cityTexture, NULL, &dst);
		FC_Draw(font, renderer, dst.x - 15.0f, dst.y * 1.0f, cit->name.c_str());

		// Printing trucks in city
		i = 0;
		foreach(tit, trucksInCity) {
			dst.y = h - 3 * (margin + elemSize);
			dst.x = currentWidth + i * (elemSize + margin) + shiftTrucks;

			SDL_RenderCopy(renderer, truckTexture, NULL, &dst);
			FC_Draw(font, renderer, dst.x - 15.0f, dst.y * 1.0f, tit->parameters[0].name.c_str());

			set<Literal> boxesInTruck = state.query(Literal(pBoxin, { vBox, tit->parameters[0] }));
			int j = 0;
			foreach(bit, boxesInTruck) {
				dst.y = h - (4 + j) * (margin + elemSize);

				SDL_RenderCopy(renderer, boxTexture, NULL, &dst);
				FC_Draw(font, renderer, dst.x - 15.0f, dst.y * 1.0f, bit->parameters[0].name.c_str());

				j++;
			}
			i++;
		}

		currentWidth += totalWidth + margin;
	}

	SDL_RenderPresent(renderer);
}
