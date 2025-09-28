
#include "app.h"

#include <argparse/argparse.hpp>

//------------------------------------------------------------------------

// --state saved_states/reference_state_00.json --output foo.png

int main(int argc, char** argv)
{
    argparse::ArgumentParser program("assignment1");
    program.add_argument("--state")
        .help("State JSON file to load on startup");
    program.add_argument("--output")
        .help("Render one frame, output image to this PNG file, terminate");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    App app;

    filesystem::path png_output = "";

    if (auto s = program.present("state"))
    {
        cerr << "Loading state from " << *s << endl;
        app.m_state.load(*s);

        if (auto o = program.present("output"))
        {
            cerr << "Instructed to save image to " << *o << " and terminate" << endl;
            png_output = *o;
        }
    }
    
    app.run(png_output);   // if argument is empty, run interactively

    return 0;
}

