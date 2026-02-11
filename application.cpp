#include "daisy_seed.h"
#include "hardware.h"
#include "perspective.h"

using namespace daisy;
using namespace perspective;

int main(void)
{
    perspective::Perspective ui;

    // Initialise the UI
    ui.Init();
    
    //ui.LightLed(true);

    ui.Exec();
}
