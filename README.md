# parrallelizedStackFocusing
Final project for Operating Systems class which takes a stack of images at different focus levels, and produces an image with all parts in maximum focus (using a simple focus algorithm, which results in degradation of image quality).

# To Use:

# 1 Install MESA and FreeGlut:

```
sudo apt-get install freeglut3-dev
sudo apt-get install mesa-common-dev
```

# 2 Input desired parameters: (hardcoded variables requested by grader)

```IN_PATH
``` - input location of image stack
```OUT_PATH
``` - input location of output image
```desiredColumns
``` - input desired number of columns for image to be split into
```desiredRows
``` - input desired number of rows for image to be split into

# 3 Build and run application:

```gcc -Wall mainv1.c gl_frontEnd.c fileIO_TGA.c -lm -lGL -lglut -o focus
```

# Version Breakdown:

Version 1 - Multithreaded with no synchronization: Divides image into rectangular regions and assigns a thread to each region. Each thread then executes an HDR algorithm.

Version 2 - Multithreaded with a single lock: Uses random sampling to compute an in-focus image. A single mutex lock guarentees synchorization.

Version 3 - Multithreaded with multiple locks: Assigns one mutex lock to each rectangular region. Avoids deadlock by implementing lock heirarchy.
