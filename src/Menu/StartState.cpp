/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "StartState.h"
#include "../version.h"
#include "../Engine/Logger.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/Action.h"
#include "../Engine/Surface.h"
#include "../Engine/Options.h"
#include "../Engine/Language.h"
#include "../Engine/Sound.h"
#include "../Engine/Music.h"
#include "../Engine/Font.h"
#include "../Engine/Timer.h"
#include "../Engine/CrossPlatform.h"
#include "../Interface/FpsCounter.h"
#include "../Interface/Cursor.h"
#include "../Interface/Text.h"
#include "MainMenuState.h"
#include "CutsceneState.h"
#include <SDL_mixer.h>
#include <SDL_thread.h>
#include <SDL_rotozoom.h>

namespace OpenXcom
{

LoadingPhase StartState::loading;
std::string StartState::error;

/**
 * Initializes all the elements in the Loading screen.
 * @param game Pointer to the core game.
 */
StartState::StartState() : _anim(0), _splash(0), _splash_set(false), _splash_mtx(0), _thread(0)
{
	//updateScale() uses newDisplayWidth/Height and needs to be set ahead of time
	Options::newDisplayWidth = Options::displayWidth;
	Options::newDisplayHeight = Options::displayHeight;
	Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, false);
	Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, false);
	Options::baseXResolution = Options::displayWidth;
	Options::baseYResolution = Options::displayHeight;
	_game->getScreen()->resetDisplay(false, true);

	// Create objects
	_splash_mtx = SDL_CreateMutex();
	loading = LOADING_STARTED;
	error = "";

	_font = new Font();
	_font->loadTerminal();
	_lang = new Language();

	_text = new Text(Options::baseXResolution, Options::baseYResolution, 0, 0);
	_cursor = new Text(_font->getWidth(), _font->getHeight(), 0, 0);
	_timer = new Timer(150);

	setPalette(_font->getPalette(), 0, 2);

	add(_text);
	add(_cursor);

	// Set up objects
	_text->initText(_font, _font, _lang);
	_text->setColor(0);
	_text->setWordWrap(true);

	_cursor->initText(_font, _font, _lang);
	_cursor->setColor(0);
	_cursor->setText("_");

	_timer->onTimer((StateHandler)&StartState::animate);
	_timer->start();

	// Hide UI
	_game->getCursor()->setVisible(false);
	_game->getFpsCounter()->setVisible(false);

	if (Options::reload)
	{
		if (Options::oxceStartUpTextMode < 2)
		{
			addLine("Restarting...");
			addLine("");
		}
	}
	else
	{
		if (Options::oxceStartUpTextMode < 2)
		{
			addLine(CrossPlatform::getDosPath() + ">openxcom");
		}
	}
}

/**
 * Kill the thread in case the game is quit early.
 */
StartState::~StartState()
{
	if (_thread) { SDL_KillThread(_thread); }
	if (_splash_mtx) { SDL_DestroyMutex(_splash_mtx); }
	delete _font;
	delete _timer;
	delete _lang;
}

/**
 * Reset and reload data.
 */
void StartState::init()
{
	State::init();

	// Silence!
	Sound::stop();
	Music::stop();
	if (!Options::mute && Options::reload)
	{
		Mix_CloseAudio();
		_game->initAudio();
	}

	// Load the game data in a separate thread
	_thread = SDL_CreateThread(load, (void *)this);
	if (_thread == 0)
	{
		// If we can't create the thread, just load it as usual
		load((void *)this);
	}
}

/**
 * If the loading fails, it shows an error, otherwise moves on to the game.
 */
void StartState::think()
{
	bool splash_notnull_edge = false;
	if (!_splash_set) { // check if splash had been set
		SDL_LockMutex(_splash_mtx);
		if (_splash)  {
			splash_notnull_edge = true;
			_splash_set = true;
		}
		SDL_UnlockMutex(_splash_mtx);
	}
	if (splash_notnull_edge) { // display splash if it had been set
		auto screen = _game->getScreen();
		double zoomX = ((double)screen->getWidth()) / _splash->getWidth();
		double zoomY = ((double)screen->getHeight()) / _splash->getHeight();
		double zoom  = zoomX < zoomY ? zoomX : zoomY;
		auto splash_sdl_zoomed = zoomSurface(_splash->getSurface(), zoom, zoom, SDL_FALSE);
		if (!splash_sdl_zoomed) {
			std::string err = "zooming splash surface failed: ";
			err += SDL_GetError();
			Log(LOG_ERROR) << err;
			throw Exception(err);
		}
		_splash->fromSDL(splash_sdl_zoomed, "splash zoomed surface"); // consumes splash_sdl_zoomed
		int destX = (screen->getWidth() - _splash->getWidth())/2;
		int destY = (screen->getHeight() - _splash->getHeight())/2;
		_splash->setX(destX);
		_splash->setY(destY);
		_text->setVisible(false);
		_cursor->setVisible(false);
		screen->setPalette(_splash->getPalette()); // this->setPalette() doesn't work.
		_surfaces.push_back(_splash); // well what can you do. no way to add() a surface w/o touching main fonts
		_splash->setVisible(true);
	}
	State::think();
	_timer->think(this, 0);

	switch (loading)
	{
	case LOADING_FAILED:
		if (_splash_set) {
			delete _surfaces[_surfaces.size() - 1];
			_surfaces.resize(_surfaces.size() - 1);
			setPalette(_font->getPalette(), 0, 2);
			_game->getScreen()->setPalette(_font->getPalette(), 0, 2);
			_text->setVisible(true);
			_cursor->setVisible(true);
		}
		CrossPlatform::flashWindow();
		addLine("");
		addLine("ERROR: " + error);
		addLine("");
		addLine("More details here: " + CrossPlatform::getLogFileName());
		addLine("Make sure OpenXcom and any mods are installed correctly.");
		addLine("");
		addLine("Press any key to continue.");
		loading = LOADING_DONE;
		break;
	case LOADING_SUCCESSFUL:
		CrossPlatform::flashWindow();
		Log(LOG_INFO) << "OpenXcom started successfully!";
		_game->setState(new GoToMainMenuState);
		if (!Options::reload && Options::playIntro)
		{
			_game->pushState(new CutsceneState("intro"));
		}
		else
		{
			Options::reload = false;
		}
		_game->getCursor()->setVisible(true);
		_game->getFpsCounter()->setVisible(Options::fpsCounter);
		break;
	default:
		break;
	}
}

/**
 * The game quits if the player presses any key when an error
 * message is on display.
 * @param action Pointer to an action.
 */
void StartState::handle(Action *action)
{
	State::handle(action);
	if (loading == LOADING_DONE)
	{
		if (action->getDetails()->type == SDL_KEYDOWN)
		{
			_game->quit();
		}
	}
}

/**
 * Blinks the cursor and spreads out terminal output.
 */
void StartState::animate()
{
	if (!_splash_set) {
		_cursor->setVisible(!_cursor->getVisible());
	}
	_anim++;

	if (loading == LOADING_STARTED)
	{
		std::ostringstream ss;
		ss << "Loading OpenXcom " << OPENXCOM_VERSION_SHORT << OPENXCOM_VERSION_GIT << "...";
		if (Options::reload)
		{
			if (Options::oxceStartUpTextMode < 2)
			{
				if (_anim == 2)
					addLine(ss.str());
			}
		}
		else
		{
			switch (_anim)
			{
			case 1:
				if (Options::oxceStartUpTextMode < 1)
				{
					addLine("DOS/4GW Protected Mode Run-time  Version 1.9");
					addLine("Copyright (c) Rational Systems, Inc. 1990-1993");
				}
				break;
			case 6:
				if (Options::oxceStartUpTextMode < 2)
				{
					addLine("");
					addLine("OpenXcom initialisation");
				}
				break;
			case 7:
				if (Options::oxceStartUpTextMode < 1)
				{
					addLine("");
					if (Options::mute)
					{
						addLine("No Sound Detected");
					}
					else
					{
						addLine("SoundBlaster Sound Effects");
						if (Options::preferredMusic == MUSIC_MIDI)
							addLine("General MIDI Music");
						else
							addLine("SoundBlaster Music");
						addLine("Base Port 220  Irq 7  Dma 1");
					}
				}
				if (Options::oxceStartUpTextMode < 2)
				{
					addLine("");
				}
				break;
			case 9:
				if (Options::oxceStartUpTextMode < 2)
				{
					addLine(ss.str());
				}
				break;
			}
		}
	}
}

/**
 * Adds a line of text to the terminal and moves
 * the cursor appropriately.
 * @param str Text line to add.
 */
void StartState::addLine(const std::string &str)
{
	_output << "\n" << str;
	_text->setText(_output.str());
	int y = _text->getTextHeight() - _font->getHeight();
	int x = _text->getTextWidth(y / _font->getHeight());
	_cursor->setX(x);
	_cursor->setY(y);
}

/**
 * Loads game data and updates status accordingly.
 * @param game_ptr Pointer to the game.
 * @return Thread status, 0 = ok
 */
int StartState::load(void *this_ptr)
{
	StartState *self = (StartState *)this_ptr;
	try
	{
		Log(LOG_INFO) << "Loading data...";
		Options::updateMods();
		if (FileMap::fileExists("splash.png")) {
			try {
				auto splashsurf = new Surface(320, 200, 0, 0);
				splashsurf->loadImage("splash.png");
				self->setSplash(splashsurf);
			} catch (Exception &e) {
				Log(LOG_ERROR) << "Error loading splash.png: " << e.what();
			}
		} else {
			Log(LOG_ERROR) << "No splash.png ";
		}
		_game->loadMods();
		Log(LOG_INFO) << "Data loaded successfully.";
		Log(LOG_INFO) << "Loading language...";
		_game->loadLanguages();
		Log(LOG_INFO) << "Language loaded successfully.";
		loading = LOADING_SUCCESSFUL;
	}
	catch (std::exception &e)
	{
		error = e.what();
		Log(LOG_ERROR) << error;
		loading = LOADING_FAILED;
	}

	return 0;
}

/**
 * Sets a splash surface.
 * Gets called from the loading thread, thus locked.
 */
void StartState::setSplash(Surface *surface)
{
	SDL_LockMutex(_splash_mtx);
	if (!_splash_set) {	_splash = surface; }
	SDL_UnlockMutex(_splash_mtx);
}

}
