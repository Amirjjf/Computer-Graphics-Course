# Aalto CS-C3100 Computer Graphics, Fall 2025

## Table of Contents
1. Creating submission packages
1. Forking the starter code from GitLab
1. Pulling changes from the upstream repo to your fork
1. Setting up the build system
    1. Windows (grading and main support platform)
    1. MacOS
    1. Linux

## Creating a submission package using Git

As per the detailed instructions provided for each round in their respective `instructions.pdf`, we request you to package everything relevant for the round in a ZIP archive and submit it on MyCourses. The archive is supposed to contain only the files necessary for compiling and running your code on the TAs' Windows machines. This is typically everything in the assignment subfolder, minus the reference executable and your `build` subdirectory, which is unnecessary for us and often runs in the 100s of MiB.

Here's a neat way to prepare the archive in a way that automatically includes everything relevant and nothing more. The prerequisites to use this method are
1. you use Git for storing both the code and any extra assets you may have included as part of extra credit work. Check the output of `git status`: If you see a listing of `Untracked files`, you have files that are not under version control, and will thus *not* be added to the archive. Do we need those files for grading? If yes, make sure they're tracked by the repo using `git add` and committed. (Having untracked files is not a problem in itself, just as long they are not needed for compiling and running your submission.)
2. you have committed all your changes to the repo, so that your HEAD revision contains the content you want to submit. You can verify this by checking the output of `git status`. If it says `nothing to commit, working tree clean`, you're good. If you get a listing after `Changes not staged for commit`, you've made changes that *will not be present in the archive*; did you really mean that?

Once you've verified the above, navigate to the subdirectory containing your assignment (here assuming the 1st) in your shell and type
```
git archive --output=STUDENTID-Assignment1-v1.zip HEAD .
```
This packages the current HEAD revision from the subtree rooted in the current directory and stores it in `STUDENTID-Assignment1-v1.zip` in the current directory. As always, you should verify that when unzipped to a clean location, your code can be configured using CMake, compiled, and run on the Windows platform. You will notice that the archive contains only the files in the repo, files matching the `export-ignore` rules defined in `.gitattributes` notwithstanding.


## Forking the starter code on GitLab

1. Pick a good location to store your code in.
    - If you are using a virtual Windows instance on `vdi.aalto.fi`, make sure you use a persistent storage like your home directory that is mapped onto the `Z:` drive. Otherwise you risk losing any work when the ephemeral storage on the instance gets destroyed upon shutdown. (Not that reasonable students would stop their working sessions without committing their code to remote source control, anyway.)
    - On a Mac/Linux, a good choice might be, for instance, a subfolder ```~/studies```.
    - To avoid cluttering the home folder, one could also put the `studies` (or similar) folder under `~/Documents`.

1. Point your browser to the [starter code distribution repository on Aalto GitLab](https://version.aalto.fi/gitlab/cs-c3100/cs-c3100-2025) and sign in using your Aalto credentials. ***Note that you have to use the "Aalto/HAKA Login" link on the left!***

1. After signing in with your Aalto credentials at the above URL, click "Fork" in the upper right corner. This creates your own version of the starter code that is linked to our version. ***In the screen that comes up, make sure the fork gets placed under your own username and not any GitLab group you might be member of. In the same screen, make sure you set your fork's visibility to "Private". This is important!***

1. Optional but ***highly recommended***: set up public key authentication for GitLab so that you do not have to enter usernames and passwords every time you pull or push code [(instructions)](https://docs.gitlab.com/ee/user/ssh.html)

## Pulling and merging changes from our distribution repository

When it's time to get the starter code for a new round --- or to patch the current assignment with changes that we've published on the distribution repo --- the clean way is to configure the fork you created in the beginning of the first
assignment so that it points to the original repository. This will, as shown below, allow you to pull changes
from there into your existing private repository instead of making a new clone for each assignment round.

In Git parlance, this is called setting an *upstream*. This happens in your terminal (on Windows, the “x64 Native
Tools Command Prompt for VS2022"). Navigate to the root directory of the fork you created and
pulled for the first assignment. (This could be, for instance, the directory `Z:\studies\cs-c3100-2025`.) Note
that this is one level higher in the hierarchy than the subdirectory where you worked on the first assignment! Once
there, set the upstream by
```
git remote add upstream git@version.aalto.fi:cs-c3100/cs-c3100-2025.git
```
Here we’re assuming you’ve set up SSH authentication to communicate with GitLab; substitute the corresponding HTTPS URL if necessary.

Now it’s time to pull in changes from our source repository through your shiny new upstream link. Before doing that, be sure to commit any pending changes you may have in your working tree and push them to your own remote. (This is to prepare for the unlikely case that problems will occur in the upcoming merge.)

With your fork now linked to its original source, you can fetch new contents from there by
```
git fetch upstream
git checkout main (⁎)
git merge upstream/main
```

The first command fetches the current HEAD revision of our upstream repository onto your local copy, but does not yet change the contents of the files in your local working tree. The second command marked by `(*)` is only necessary if you have created local branches in your fork. Finally, the third command merges the changes from upstream to your local repository, so that your main branch will now contain both the work you did on the earlier assignment(s) and the updated code from our distribution repository. There should be no merge conflicts; if there are, you can go back to the pre-merge state by ```git merge --abort```.

## Setting up the build system

### Windows and Visual Studio 2022 (main supported platform)

1. Make sure you have Visual Studio 2022 (the freely available Community Edition suffices) installed with C++ development options enabled [(link)](https://visualstudio.microsoft.com/).

1. Clone your shiny new fork by opening "x64 Native Tools Command Prompt for VS2022" from Start Menu, navigating to your location of choice, and running `git clone` with your repo's address. ***Replace [USERNAME] with your own.*** On a VDI virtual machine, this could look like:

    ```
    C:\Program Files\Microsoft Visual Studio\2022\Community>z:
    Z:\>cd studies
    Z:\studies>git clone https://version.aalto.fi/gitlab/[USERNAME]/cs-c3100-2025.git
    ```

1. Prepare the code for building using the CMake build system generator.

    1. Open "x64 Native Tools Command Prompt for VS2022" from the Windows Start Menu. *The x64 bit is important! There is also an x86 version.*

    2. In the command prompt, navigate to the directory `assignment1` cloned from GitLab and run the CMake configuration step for the `configure-vs2022` preset. If, for instance, you cloned the repo on a VM into your home directory under `studies\cs3100\assignment1`, this could look like:
        ```
        C:\Program Files\Microsoft Visual Studio\2022\Community>z:
        Z:\>cd studies\cs-c3100-2025\assignment1
        Z:\studies\cs-c3100-2025\assignment1>cmake -B build --preset=configure-vs2022
        ```

        The last step downloads the necessary external libraries using the `vcpkg` package manager, and generates a Visual Studio solution file `build\assignment1.sln`.

1. In Windows explorer, double click the file `build\assignment1.sln`. This should open the code in Visual Studio 2022. Choose **Build > Build Solution**. Once the build finishes, run your program by pressing **F5**. You should see a triangle floating above a reference plane, like in the reference. Try clicking on the buttons in the control window to change the geometry and shading mode.


### MacOS (best-effort support only)

The `vcpkg` configuration that we provide also includes presets for generating XCode projects on MacOS, as well as presets for building Debug and Release builds directly for use on, e.g., Linux systems. Like with all content-related questions, we recommend asking actively on the assignment's Slack channel for pointers and help if you run into issues.

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

1. Install the ```vcpkg``` package manager into a suitable directory (here ```~/dev/vcpkg```):

    First, pull and initialize by
    ```
    mkdir -p ~/dev && cd ~/dev
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ```

    Then set up environment variables to enable easy calling by
    ```
    echo 'export VCPKG_ROOT="$HOME/dev/vcpkg"' >> ~/.zshrc
    echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.zshrc
    ```

    Finally, restart your terminal to enable changes to ```.zshrc``` to take effect.

    Typing ```vcpkg``` in a new terminal window should now show its usage.

1. Create your own fork of the starter code per the instructions at the top of this page.

1. Find a suitable directory (here we assume ```~/studies``` already created) and pull the starter code from GitLab, remembering to ***replace [USERNAME] with your own:***
    ```
    cd ~/studies
    git clone https://version.aalto.fi/gitlab/[USERNAME]/cs-c3100-2025.git
    ```

1. For each assignment --- here assuming the first one --- use ```vcpkg``` to generate a project file for Xcode by
    ```
    cd cs-c3100-2025/assignment1
    cmake -B build --preset=configure-xcode
    ```

1. Fire up Xcode, open the project file ```~/studies/cs-c3100-2025/assignment1/build/```, and start coding!


### Linux (best-effort support only)

The project includes CMake presets for preparing (configuring) Debug and Release builds on Linux. For help, like with all content-related questions, we recommend asking actively on the assignment's Slack channel for pointers and help if you run into issues. Online resources can also be helpful with distro-specific issues.

1. This assumes you have some basic tools installed, such as CMake, git, a text editor and a C/C++ compiler. A build generator such as make or ninja is also necessary.

1. Install the `vcpkg` package manager into a suitable directory (here `~/dev/vcpkg`):
    Note: Some distributions have vcpkg in their package repositories, but at least in some cases that version hasn't worked as well as the manual method. You're free to try the repository version, though, as it might be an easier way to install the software.

    First, pull and initialize by
    ```
    mkdir -p ~/dev && cd ~/dev
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh -disableMetrics
    ```

    `-disableMetrics` is optional, but adding it here makes vcpkg opt out of telemetry that is otherwise collected whenever running the program.

    Then set up environment variables to enable calling vcpkg more easily:
    ```
    export VCPKG_ROOT="$HOME/dev/vcpkg"
    export PATH="$VCPKG_ROOT:$PATH"
    ```

    You can add the exports into your shell specific file (~/.bashrc, ~/.zshrc, ...), or in ~/.profile for access across all shells as your current user.
    This could be done by piping the text into the file:
    ```
    echo 'export VCPKG_ROOT="$HOME/dev/vcpkg"' >> ~/.profile
    echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.profile
    ```
    ...or just editing the file with your preferred editor.

    Finally, relaunch your terminal to enable changes to the shell configuration to take effect, or log out and back in for changes to `.profile` to take effect. Typing `vcpkg` in a new terminal window should now show its usage.

    For future reference, to update vcpkg packages, pull the latest changes and rerun the bootstrap script:
    ```
    cd ~/dev/vcpkg
    git pull
    ./bootstrap-vcpkg.sh -disableMetrics
    ```

1. Create your own fork of the starter code per the instructions at the top of this page.

1. Find a suitable directory (here we assume `~/studies` already created) and pull the starter code from GitLab, remembering to ***replace [USERNAME] with your own:***
    ```
    cd ~/studies
    git clone https://version.aalto.fi/gitlab/[USERNAME]/cs-c3100-2025.git
    ```

1. For each assignment --- here assuming the first one --- use ```cmake``` to configure and build the project in Debug mode:
    ```
    cd cs-c3100-2025/assignment1
    cmake -B build --preset=Debug
    cmake --build
    ```
    To get an optimized build, reconfigure with `cmake -B build --preset=Release`, then build (`cmake --build`). If not changing between the Release and Debug configurations, only the build step is necessary for subsequent builds. There is also a cmake extension for VSCode that allows configuring and building the project in the editor UI.

1. Fire up your preferred editor, open the project sources in `~/studies/cs-c3100-2025/assignment1/src/`, and start coding!
