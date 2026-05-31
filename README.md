# ElementRuntime

Unreal Engine plugin that integrates the [Element](https://github.com/vladbelousoff/element.git) ECS core and the [Oak](https://github.com/vladbelousoff/oak.git) scripting runtime into Unreal, wired together through [element-oak-bridge](https://github.com/vladbelousoff/element-oak-bridge.git).

It lets you drive entity/component simulation from a parallel ECS scheduler inside Unreal and author systems in Oak script, with the results rendered through ordinary Unreal actors.

## What's Inside

- **`FElementRuntimeScheduler`** (`Public/ElementRuntime.h`) — a conflict-aware parallel scheduler that runs `elm::System` batches on Unreal's task graph, precomputing batch assignments so per-frame scheduling is free.
- **Oak bridge glue** (`ElementOakBridge.*`) — loads `.oak` programs, registers native component types, and turns `@ElementSystem` functions into `elm::System` values.
- **`AElementBoidsActor`** (`ElementBoidsActor.*`) — a sample actor that spawns and renders an ECS simulation via instanced static meshes, with parameters exposed in the editor.
- **Content/Scripts** — example Oak scripts (`zombies.oak`, `math/vec3.oak`).

The `ElementRuntime` module is a `Runtime` module built with C++20, RTTI disabled, and exceptions disabled.

## Dependencies

This plugin bundles its native dependencies as git submodules:

| Path | Repository | Role |
| --- | --- | --- |
| `element/` | [element](https://github.com/vladbelousoff/element.git) | Header-only ECS core |
| `element-oak-bridge/` | [element-oak-bridge](https://github.com/vladbelousoff/element-oak-bridge.git) | Bridges the ECS to the Oak VM |
| `oak/` | [oak](https://github.com/vladbelousoff/oak.git) | Scripting language and runtime |

Clone with submodules:

```sh
git clone --recurse-submodules git@github.com:vladbelousoff/ElementRuntime.git
```

If you already cloned without `--recurse-submodules`:

```sh
git submodule update --init --recursive
```

## Building

Drop this plugin under your project's `Plugins/` directory (or use it as a submodule, as the [Element](https://github.com/vladbelousoff/element.git) project does) and regenerate project files. `ElementRuntime.Build.cs` resolves include paths from the bundled submodules by default, so no extra setup is required.

To point at checkouts or install roots elsewhere, set any of these environment variables before building:

| Variable | Overrides | Default |
| --- | --- | --- |
| `ELEMENT_ROOT` | Element include root | `<plugin>/element` |
| `ELEMENT_OAK_BRIDGE_ROOT` | Bridge include root | `<plugin>/element-oak-bridge` |
| `OAK_ROOT` | Oak source root | `<plugin>/oak` |

The build defines `OAK_ATOMIC_REFCOUNT=1` and `OAK_STATIC=1` so the Oak runtime is compiled into the module statically with atomic refcounting.

## Usage

Place an `AElementBoidsActor` in a level and adjust its `Element` / `Element|Camera` / `Element|Flamethrower` properties in the editor. On begin play the actor builds the ECS world, loads its Oak scripts, and steps the simulation through `FElementRuntimeScheduler` each tick.

See the [Element](https://github.com/vladbelousoff/element.git) README for ECS and scheduler concepts, and the [element-oak-bridge](https://github.com/vladbelousoff/element-oak-bridge.git) README for the Oak integration API.
