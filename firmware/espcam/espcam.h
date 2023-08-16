#ifndef __ESPCAM_H__
#define __ESPCAM_H__

#include "Commands.h"

class ESPCam
{
public:
    class Commands : public BaseCommands<ESPCam>
    {
    public:
        Commands(ESPCam* parent, bool flag) : BaseCommands(parent, flag) {}
    };
};

#endif