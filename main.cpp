#include "Arena.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>\n";
        return 1;
    }

    Arena arena;
    if (!arena.load_config(argv[1])) return 1;
    if (!arena.load_robots())        return 1;
    arena.place_obstacles();
    arena.place_robots();
    arena.run();
    return 0;
}
