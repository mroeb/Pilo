
#include "Pilo.h"

int main()
{
    Pilo::GPIO<"/dev/gpiochip0", "PiloGPIO"> gpio{};

    gpio.add_lines<Pilo::Direction::Input, 17>();
    gpio.add_lines<Pilo::Direction::Output, 27>();

    gpio.write<17>(true);
    if (gpio.read<27>())
    {
        std::cout << "Active\n";
    }

    return 0;
}
