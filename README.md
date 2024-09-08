# ParentChildWindows: Interprocess Communication & Graphics with X11 Library

**ParentChildWindows** is a C program utilizing the X11 library to create an interactive window featuring a rotating square. This project demonstrates interprocess communication through shared memory, showcasing the parent-child relationship between processes.

## Key Features

- **Dual Process Execution**: Operates in two modes—parent and child processes—to exhibit interprocess communication functionalities.
- **Shared Memory Utilization**: Utilizes shared memory via the X11 library, passing information between parent and child processes.
- **Window Creation and Control**: Creates and manages windows, handling rendering and event-driven interactions using Xlib.
- **Thread Management**: Implements pthreads to manage the rendering of the rotating square in the child process.
- **User Interaction**: Supports user actions, enabling square color fill upon click and closure upon right-click within the square.

## Functionality Overview

When executed as a child process, the program receives a shared memory ID from the parent process via `argv[2]`, creates a new window, and initiates a thread responsible for the continuous rendering of the rotating square. When executed as a parent process, it initializes the shared memory ID, establishes the window, and initiates the primary event loop.

## License

This project is not licensed.
