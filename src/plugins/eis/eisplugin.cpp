#include "eisplugin.h"

#include "inputbackend.h"

#include "input.h"

EisPlugin::EisPlugin()
    : Plugin()
{
    KWin::input()->addInputBackend(std::make_unique<KWin::EisBackend>());
}

EisPlugin::~EisPlugin()
{
}
