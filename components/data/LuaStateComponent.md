# [LuaStateComponent](LuaStateComponent.hpp)

`Component` that holds a lua state in the form of a [sol::state](https://github.com/ThePhD/sol2) object.

## Specs

* Not [reflectible](https://github.com/phisko/putils/blob/master/reflection.md)
* Not serializable (contains pointers)
* Used by the [LuaSystem](../../systems/LuaSystem.md)

## Usage

An `Entity` with a `LuaStateComponent` is created by the [LuaSystem](../../systems/LuaSystem.md) during initialization. It is then accessible by users to perform any desired operations with lua (executing scripts, evaluating lua state variables/expressions, or even registering new types and functions).

Note that the `LuaSystem` already provides a set of helper functions to easily register new types and functions. These helper functions should be preferred over direct manipulation of the `LuaStateComponent`.