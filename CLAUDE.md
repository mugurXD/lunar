This project is composed of two core parts:
1. **lunar** - the game engine written in C++. This engine uses the dependencies from the folder "deps". Notably, it is an engine that uses Vulkan for rendering, an ECS system, and other custom functionalities in order to be flexible and easy to mod.
2. **trok** - the game built on top of **lunar**. This game is still in a very early alpha state, but there are a few core concepts which it is built around:
    * The world is meant to be infinite, with different climates, biomes and features;
    * There are many towns scattered throughout this world. Right now, towns are only implemented as "center points", but actual town generation is not done. Actual random town generation is meant to be implemented sooner or later.
    * Roads are also generated randomly. There are different types of roads, defined in a json file, such as a national road. Based on the properties defined, a pathfinding algorithm is used to generate roads the player has to drive on. Right now, this is in a relatively decent state.
    * The player drives a truck. This is the core of the game. Right now, the player gets to do deliveries from a town to another. The main accent of the game is **exploration**. The player has to have a reason to leave a town and go explore, besides just doing deliveries, which will probably be implemented by the user getting "heat" from the police and being chased if they did too many deliveries around a specific city.
    * The player will not receive a GPS with a huge map telling them where to go. A minimap will likely be present, but the player will have to guide themselves on the map based on their knowledge and navigation skills. There will be many routes with different risk/reward ratios.
    * The player must keep the cargo intact.
    * Right now, the player can only drive. One idea would be for the player to unlock different "abilities", that don't really have to be realistic, similar to the two games of The Legend of Zelda on the Nintendo Switch (BOTW and TOTK). They will be able to use these abilities to traverse the map and avoid obstacles.
    * Some obstacle ideas: police, wildlife (if off-road), roadworks

The files for **lunar** are in the "src" and "inc" folders.
The files for **trok** are in the "trok" folder.
The folder "resources" contains data necessary at runtime for the game.

Do not write any code at all unless explicitly requested. The main reason you are used is for guidance and tips, not actual implementation.

When you do write code, follow these guidelines:

1. Use the following code style:
- snake_case for local variables;
- PascalCase for class/struct names, global functions;
- camelCase for method names and class/struct member variables;
- Use tabs for indentation;
- Align, using spaces, the parameters of function calls and definitions that span multiple lines;
2. Do not write comments in your code unless explicitly requested. If you do write comments, they must explain WHY something was designed in a certain way, not what the code itself is doing;
3. Systems/code should be reused as much as possible, and not duplicated;
4. Try to write as little code as possible in order to achieve the desired goal.
5. Avoid using magic numbers or strings; instead, define them as constants with descriptive names.
6. Avoid using global variables unless absolutely necessary; prefer passing parameters to functions or using class/struct member variables.
7. Use descriptive names for variables, functions, and classes/structs that clearly indicate their purpose.
8. Avoid deep nesting of code; if a function is too complex, consider breaking it into smaller helper functions.
9. Changes to a certain function/class/struct should be done in a manner that does not require changing the code in order parts of the project for it to work.
10. Avoid using language-specific features that may not be portable across different platforms or compilers.
11. Try to make use of already existing classes/structs/functions/objects available in the codebase (in this order: project specific - libraries used in the project - standard library), instead of adding new code to do the same thing;
12. Use a const-first approach; everything should be declared as constant unless it is explicitly meant to be modified;
13. Avoid using exceptions for control flow; use them only for exceptional cases;
14. Ask for clarification if you are unsure about the requirements or the expected behavior of a certain piece of code.
15. Ask for permission whenever doing something that may have a significant impact on the project, such as changing the architecture, adding a new dependency, or modifying a core component.
16. Prefer using modern C++ features and idioms over older C++ constructs.