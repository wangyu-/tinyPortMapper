#pragma once

#include "my_ev_common.h"
#include "ev.h"

//#if defined(__MINGW32__)
//#define my_ev_io_set(ev,fd_,events_)    ev_io_set(ev,_open_osfhandle(fd_,0),events_)
//#define my_ev_io_init(ev,cb,fd,events)    ev_io_init(ev,cb,_open_osfhandle(fd,0),events)   
//#else
//#define my_ev_io_set(ev,fd_,events_)    ev_io_set(ev,fd_,events_)
//#define my_ev_io_init(ev,cb,fd,events)    ev_io_init(ev,cb,fd,events)   
//#endif
