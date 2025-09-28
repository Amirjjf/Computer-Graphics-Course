# CS-C3100 Computer Graphics, Fall 2025
# Lehtinen / Kemppinen, Kallio, Kautto, Míchal
#
# Assignment 1: Warmup / Introduction

Student name: Amirreza Jafariandehkordi
Student number: 103574967
Hours spent on requirements (approx.): 3
Hours spent on extra credit (approx.): 5

# First, some questions about where you come from and how you got started.
# Your answers in this section will be used to improve the course.
# They will not be judged or affect your points, but please answer all of them.
# Keep it short; answering shouldn't take you more than 5-10 minutes.

- What are you studying here at Aalto? (Department, major, minor...?) Computer Science

- Which year of your studies is this? First

- Is this a mandatory course for you? No

- Have you had something to do with graphics before? Other studies, personal interests, work?
Nothing 

- Do you remember basic linear algebra? Matrix and vector multiplication, cross product, that sort of thing?
Little

- How is your overall programming experience? What language are you most comfortable with?
Javascript and typescript are my favs, I am good at them and also good at python 

- Do you have some experience with these things? Which versions of them? (If not, do you have experience with something similar such as C or Direct3D?)
C++:not really, just a little
OpenGL: no experience
I knew C but not much 

- Have you used a version control system such as Git, Mercurial or Subversion? Which ones?
yes, git

- Did you work on the assignment using Aalto computers, your own computers, or both? my own

# Which parts of the assignment did you complete? Mark them 'done'.
# You can also mark non-completed parts as 'attempted' if you spent a fair amount of
# effort on them. If you do, explain the work you did in the problems/bugs section
# and leave your 'attempt' code in place (commented out if necessary) so we can see it.

(Try to get everything done! Based on previous data, virtually everyone who put in the work and did well in the first two assignments ended up finishing the course, and also reported a high level of satisfaction at the end of the course.)

                            opened this file (0p): done
                         R1 Moving an object (1p): done
R2 Generating a simple cone mesh and normals (3p): done
  R3 Converting mesh data for OpenGL viewing (3p): done
           R4 Loading a large mesh from file (3p): done


# Implemented Features

- All required features are implemented.

- R1: Model translation with keyboard (arrows, PgUp/PgDn)
- R2: Procedural cone mesh generation with correct normals
- R3: Indexed mesh unpacking for OpenGL
- R4: OBJ and PLY file loading (UI button and drag-and-drop)
- Model rotation (Y axis), non-uniform scaling (X), and reset controls
- Camera: trackball rotation, target decoupling, FOV X slider, animation toggle
- Efficient normal transformation (uNormalMatrix uniform)
- Shading toggle (flat/smooth)
- State save/load (F1..F12, Shift/Ctrl modifiers)

# Extra Credit / Advanced Features

- all implemented except for Rendering Signed Distance Fields and I attempted it so much

- Mesh simplification using Quadric Error Metrics (QEM), with interactive slider and button
- All generated/loaded meshes can be simplified and manipulated with the same controls

# Usage

- Open the app and use the Controls window (right panel):
  - Load models: Use the "Load Triangle Model", "Load Indexed Model", "Load Generated Cone", "Load OBJ model (L)", or "Load PLY model" buttons.
  - Move the model: Use arrow keys and PgUp/PgDn.
  - Rotate (Y axis): Q/E keys. Scale X: Z/X keys. Reset: = key.
  - Camera: Drag with left mouse (trackball), use FOV X slider, HOME/END for yaw, and R to toggle camera animation.
  - Shading: Toggle with S key or checkbox.
  - Save/load state: F1..F12 (load), Shift+F1..F12 (save), Ctrl+F1..F12 (load reference).
  - Simplify mesh: See "Triangles" count, adjust "Target triangles" slider, and click "Simplify (QEM)".
  - Take screenshot: Use the "Take screenshot" button.

# Known issues

(None critical; all main features work. For very high SDF resolutions, simplification is recommended for performance.)

# Are there any known problems/bugs remaining in your code?

(Please provide a list of the problems. If possible, describe what you think the cause is, how you have attempted to diagnose or fix the problem, and how you would attempt to diagnose or fix it if you had more time or motivation. This is important: we are more likely to assign partial credit if you help us understand what's going on.)

# Did you collaborate with anyone in the class? no

(Did you help others? Did others help you? Let us know who you talked to, and what sort of help you gave or received.)

# Did you use any AI tools to complete the assignment? yes, AI helped me in the extra credits especially the hard questions such as shading. 

(What did you use and how? We're interested in your experiences to get an idea of how to best accommodate for the future.)

# Any other comments you'd like to share about the assignment or the course so far?

(Was the assignment too long? Too hard? Fun or boring? Did you learn something, or was it a total waste of time? Can we do something differently to help you learn? Please be brutally honest; we won't take it personally.) seems way too hard for me tbh idk. I've had years of experience as a full stack dev but these stuff were hard 

