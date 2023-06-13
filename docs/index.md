# Maidenless Engine

Maidenless Engine is a slim game engine allowing game creators to rapidly iterate through an opiniated set of game routines. Maidenless doesn't aim to be general purpose. Its main goal is to create first person shooters. Creating a game means modifying a given base by:

* Adding weapons, levels, enemies, and a storyline.
* Changing game existing parameters (camera controller, etc).
* Modifying shaders, and post-processing.

The base itself is a demo of the "Zombie Hierarchy" game, expected to be release soon :material-trademark:.

## Features

Existing set of features are described here, some can't be disabled.

* **Real-time ray-traced shadows** (using related Vulkan extensions). A fallback exists.
* **Mesh collider** built-in. Can be generated from the 3D mesh of a model, or by specifying a specific 3D mesh.
* **Dev-mode** to enter god mode, spawn entities, or change current level.
* **3D sound**.

Expected features to be added in the near future.

* **User interface**.
* **Animation**.
