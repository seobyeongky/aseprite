#ifndef APP_UI_PLAYABLE_H_INCLUDED
#define APP_UI_PLAYABLE_H_INCLUDED
#pragma once

#include "doc/frame.h"

namespace app {
	using namespace doc;
	
	class Playable {
	public:
	    virtual frame_t frame() = 0;
		virtual void setFrame(frame_t frame) = 0;
		virtual void play(const bool playOnce,
	              const bool playAll) = 0;
	    virtual void stop() = 0;
	    virtual bool isPlaying() const = 0;
	};
}


#endif