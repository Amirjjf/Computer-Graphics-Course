# Computer Graphics Course - CS-C3100
This repository contains computer graphics assignments implementing various rendering techniques and algorithms.

## Table of Contents
1. Creating submission packages
1. Setting up the build system
    1. Windows (main support platform)
    1. MacOS
    1. Linux

## Creating a submission package using Git

To create a submission package, use Git to automatically include everything relevant and nothing more. Prerequisites:
1. All code and assets are tracked by Git (check with `git status` - no untracked files needed for compilation)
2. All changes are committed (check with `git status` - should show "nothing to commit, working tree clean")

Navigate to the assignment subdirectory and create the archive:
```
git archive --output=Assignment1-v1.zip HEAD .
```
This packages the current HEAD revision from the current directory. Verify the archive by unzipping to a clean location and ensuring the code can be configured with CMake and compiled.




## Setting up the build system

This section covers setting up the build environment for each platform. 

In Git parlance, this is called setting an *upstream*. This happens in your terminal (on Windows, the “x64 Native
Tools Command Prompt for VS2022"). Navigate to the root directory of the fork you created and
pulled for the first assignment. (This could be, for instance, the directory `Z:\studies\cs-c3100-2025`.) Note
that this is one level higher in the hierarchy than the subdirectory where you worked on the first assignment! Once
there, set the upstream by

Here we’re assuming you’ve set up SSH authentication to communicate with GitLab; substitute the corresponding HTTPS URL if necessary.

Now it’s time to pull in changes from our source repository through your shiny new upstream link. Before doing that, be sure to commit any pending changes you may have in your working tree and push them to your own remote. (This is to prepare for the unlikely case that problems will occur in the upcoming merge.)



### Windows and Visual Studio 2022 (main supported platform)

1. Make sure you have Visual Studio 2022 (the freely available Community Edition suffices) installed with C++ development options enabled [(link)](https://visualstudio.microsoft.com/).

1. Open "x64 Native Tools Command Prompt for VS2022" from the Windows Start Menu. *The x64 bit is important! There is also an x86 version.*

1. Navigate to the assignment directory and run the CMake configuration step:
    ```
    cd assignment1
    cmake -B build --preset=configure-vs2022
    ```
    This downloads the necessary external libraries using the `vcpkg` package manager and generates a Visual Studio solution file `build\assignment1.sln`.

1. Open the solution file `build\assignment1.sln` in Visual Studio 2022. Choose **Build > Build Solution**. Once the build finishes, run your program by pressing **F5**.


### MacOS

The `vcpkg` configuration includes presets for generating XCode projects on MacOS, as well as presets for building Debug and Release builds directly.

1. Install Xcode from the Mac App Store.

1. In the Terminal, install Command Line Tools (compilers, /usr/bin/git, headers) by running
    ```
    xcode-select --install
    ```

    Still in the Terminal, open Xcode once to finish setup, then accept the license by
    ```
    sudo xcodebuild -license accept
    ```

    Verify Apple’s Git & Clang (should say "Apple Git" and "Apple Clang"):
    ```
    git --version
    clang --version
    ```

1. Install the ```vcpkg``` package manager:
    ```
    mkdir -p ~/dev && cd ~/dev
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ```

    Set up environment variables:
    ```
    echo 'export VCPKG_ROOT="$HOME/dev/vcpkg"' >> ~/.zshrc
    echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.zshrc
    ```
    Restart your terminal for changes to take effect.

1. Navigate to an assignment directory and configure the project:
    ```
    cd assignment1
    cmake -B build --preset=configure-xcode
    ```

1. Open the project in Xcode and start coding!


### Linux

The project includes CMake presets for preparing Debug and Release builds on Linux.

1. Ensure you have basic tools installed: CMake, git, a text editor, C/C++ compiler, and a build generator (make or ninja).

1. Install the `vcpkg` package manager:
    ```
    mkdir -p ~/dev && cd ~/dev
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh -disableMetrics
    ```

    Set up environment variables:
    ```
    echo 'export VCPKG_ROOT="$HOME/dev/vcpkg"' >> ~/.profile
    echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.profile
    ```
    Relaunch your terminal for changes to take effect.

1. Navigate to an assignment directory and configure/build the project:
    ```
    cd assignment1
    cmake -B build --preset=Debug
    cmake --build build
    ```
    For an optimized build, use `--preset=Release` instead.

1. Open the project sources in your preferred editor and start coding!
