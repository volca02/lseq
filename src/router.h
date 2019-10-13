#pragma once

#include "jackmidi.h"

/// midi event router. Connects to specified output port for midi output
/// receives midi events with/without time markings to be output there.
class Router {
public:
    Router(jack::Client &client, const char *outport_name)
            : client(client)
            , outport()
    {
    }

protected:
    jack::Client &client;
    jack::Port inport; // general input port for midi devices (i.e. a midi keyboard)
    jack::Port outport;
};
