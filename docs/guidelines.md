# Coding Guidelines

This document describes how code should be written as part of this project.

Also see: https://docs.google.com/document/d/1dJW-E6SN41XQuvidHRhp41O_1mJmP90bar1egZ31c10/edit?usp=sharing


**Table of Contents**
<!-- TOC start (generated with https://github.com/derlin/bitdowntoc) -->

- [Project coding style](#project-coding-style)
   * [Naming](#naming)
      + [Variables prefixes](#variables-prefixes)
      + [Type Aliases](#type-aliases)
      + [Special Types - IDs and Entities](#special-types-ids-and-entities)
      + [Special types - Context](#special-types-context)
      + [Special types - Systems](#special-types-systems)
      + [Special types - archaic](#special-types-archaic)
   * [Function Argument order](#function-argument-order)
   * [Use of auto keyword](#use-of-auto-keyword)
   * [Do not use a variable after `std::move`](#do-not-use-a-variable-after-stdmove)
   * [Avoid modifying variables within `if` conditions](#avoid-modifying-variables-within-if-conditions)
   * [Avoid certain C++ features](#avoid-certain-c-features)
- [Appendix: Tips for commenting](#appendix-tips-for-commenting)

<!-- TOC end -->

## Project coding style

* K&R with 4-space indentation for code blocks
* 2 indentation levels (8-spaces) for splitting long lines after an openning brace
* 100 column line limit
* Right-side `const`
* Reference ampersand and pointer asterisk by the variable names (right side)
* Align anything with spacing if it improves readability
* Doxygen documentation
* Spaces around NOT operator (`!`)
* Always use curly braces after if/for/while statements
* Prefer `using` over `typedef` for type aliases

```cpp

/**
 * @brief Put brief description of what this does here
 *
 * Put longer description here.
 * The parameters below can be labeled with [in], [out], or [ref].
 *
 * @param rParamA [ref] Put info about the first parameter here.
 * @param rParamA [in] Put info about the second parameter here.
 */
void short_function(Foo &rParamA, int const count)
{
    bool const condition = count < rParamA.size;
    if ( ! condition )
    {
        do_thing();
    }
}

void lots_of_parameters(
        Foo     const &parameterA
        FooFoo  const &parameterB
        FooBar  const &parameterC
        BarFoo        parameterD
        BarBar        &rParameterE
        Hamburger     &&food)
{
    // code...
}

// Not code. no need to put curly braces on a new line
constexpr std::array<float, 9> sc_table = {{
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f
}};

```

Balance spacing around parentheses. Spaces on after the opening brace should match its closing brace:

```cpp
(  a + b * (c(d) + e) + f(g)  )
```



Always use type aliases to avoid manually writing out function pointer and pointer-to-pointer types.

```cpp
using ExampleFunc_t = int(*)(std::vector<int>&) noexcept;

int example_func(std::vector<int>&) noexcept;

ExampleFunc_t test = &example_func;



// (For the few cases where even need to use raw pointers to begin with)
using IntPtr_t      = int*;
using IntConstPtr_t = int const*;

IntPtr_t const *pConstPointerToPointer;

// This would have been int const * const * pPointerToConstPointer;
IntConstPtr_t const *pConstPointerToConstPointer;
```


### Naming

* Variables are `camelCase` (with prefixes, read section below)
* Functions are `snake_case`
  * Functions that check a condition and return a bool usually start with `is_`
* Type names are `PascalCase`
  * Prefix with `E` for enums
  * Prefix with `I` for interface classes
* Macros are `SCREAMING_SNAKE_CASE`

Names should be descriptive, though single-letter names are permitted for loop counters and math-heavy expressions.


#### Variables prefixes

Hungarian notation is used to some extent. This makes it easier to tell the type when it's important. In many cases, these are things the IDE may not highlight:

Variable before underscore:

* `s_` for static
* `g_` for global
* `t_` for thread-local
* `m_` for members. Allowed to omit this if the struct/type has few or no member functions.
* `c_` for constants. Either compile-time or const determined at startup, unchanged throughout all of execution (not just for every const variable).
* Combine them (in order). commonly `gc_` for global constant,  `smc_` for static member constant.

Variable prefix by type, after underscore:

* `p` for raw pointers and smart pointers
* `r` for reference. Allowed to omit for const reference
* `o` for optionals

Examples:

* `variableName` - Regular variable
* `rVariableName` - Reference
* `m_pVariableName` - Member variable that's a pointer
* `gc_variableName` - Global constant

#### Type Aliases

* `_t` postfix for most type aliases
* `Func_t` postfix for function pointer aliases
* `Ptr_t` postfix for raw or smart pointer aliases
* `Vec_t` postfix for std::vector aliases

#### Special Types - IDs and Entities

* `Idx` postfix for array indices.
* `Id` postfix for simple instances.
* `Ent` postfix for Entities.

IDs and Entities represent an instance of some object. Data can be assigned to them by using them as array indices or keys to containers. These are **heavily** used across the codebase.

Entity refers to an Entity-Component-System (ECS) entity. These are the same as IDs, but are intended to be assigned arbitrary data by multiple subsystems and are more general-purpose.

Only two entities are defined: `DrawEnt` and `ActiveEnt`.

IDs and Entities are usually ```osp::StrongId<...>``` types. This makes them harder to accidentally mix up and adds some convenient features.

Some Id types are simply aliases to an integer type (eg: `using ObjId = std::uint32_t;`).


#### Special types - Context

`ACtx` "Active Context" prefix for structs/classes that intend to add context to an ActiveScene.

The definition of "context variable" varies across computing. Here, it refers to arbitrary data assigned to something that is currently running, and has a lifetime bound to it. For `ACtx`, this is data assigned to an ActiveScene.

`ACtx` types are usually stored as Top Data.

eg: `ACtxSceneGraph` adds scene graph support to an ActiveScene. It stores a parent-child hierarchy for ActiveEnts.

#### Special types - Systems

`Sys` "System" prefix for static System classes.

System refers to an Entity-Component-System (ECS) System. These are intended to operate on an ActiveScene (or potentially any kind of 'world') to help update or step it through time. Systems cannot be initialized, and only provide a set of related static functions. It's practically a namespace.

The existence of system classes is questionable.

#### Special types - archaic

`AComp` prefix.

An Entity-Component-System (ECS) Component. Data assigned to an Entity. No longer used.

### Function Argument order

This is a suggestion

1. **Subject** - What does this function operate on?
2. **Context** - What is the subject part of?
3. **Predicates & functors** - Put these are the end for readability

```cpp
do_something_with(rocket, vehicle, world, [] ()
{
    /* functor */
});
```

### Use of auto keyword

Allowed for `auto`:
* Returned from a function that implies the type (eg: `get<TypeName>`, `TypeName::create()`, or `create_typename()`)
* Iterators
* Integers, but only if...
  * the variable named implies an integer (eg: contains `size`, `capacity`, `length`, or `count`)
  * returned from a function that implies an integer (similar names to above)
  * clearly a result of an arithmetic expression
* Structure bindings
* Template types while metaprogramming
* Painfully long type names
* You have a good reason to


### Do not use a variable after `std::move`

Move semantics is a powerful feature that ensures that a value can be moved to a different memory location in a methodical way (instead of copying to a new location and deleting the previous value). This gives us several benefits:

* Improves performance by preventing a potentially expensive copy.
* A mechanism to ensure that only one instance of a class exists and is unique (eg: for `std::unique_ptr<T>` and `lgrn::IdOwner<T>`)

When a variable is `std::move`-ed away, it must no longer be used. It's in an undefined state; treat it like an invalid reference or iterator. This also keeps some cognitive load down when `std::move` has a consistent meaning.

Using a variable after it has been moved is technically still allowed in C++; there is no borrow checker. The compiler can catch and warn about use-after-move; static analysis (clang-tidy) can catch more complicated cases.

```cpp
std::string buffer;
do_something(buffer);
write_to_output(std::move(buffer));

// buffer is now moved, don't ever use it  
```

When objects are intended to be used again after 'move', then use `std::exchange`:

```cpp
std::string buffer;
while (...)
{
    do_something(buffer);
    write_to_output(std::exchange(buffer, {}));
}
```


### Avoid modifying variables within `if` conditions

This includes functions with side effects, or operators like `=` or `++`.

Avoid case of using short-circuit as 'logic':
```cpp
// We may expect do_something() and do_something_else() to both be called
if (conditionA && do_something() && do_something_else())
{
    // ...
}

// But no, there is hidden control flow here. Example above is equivalent to:
if (conditionA)
{
    if (do_something())
    {
        if (do_something_else())
        {
            // ...
        }
    }
}
```

Short-circuiting is for optimization only!

```cpp
// This is okay
if (simpleCondition && expensive_calculation())
{
    // ...
}
```


### Avoid certain C++ features

* Avoid C++20 ranges for the time being.
  * Controversial and had compile issues in the past.
  * BDFL finds it has a lack of features, impossible to read, and hard for others to grasp. The standard needs work tbh.
* Trailing return type, except for complicated return types or metaprogramming reasons.
  * 'consistency' is controversial, since we'd have to switch to using this everywhere to be 'consistent'
  * `override`, `const`, and `noexcept` keywords after the function is oddly placed.
* Avoid `std::pair` if possible.
  * Use a struct. Structure binding works on plain structs anyways.
  * `first` and `second` aren't very descriptive names.
  * Slow compile time.
* Avoid `std::tuple` if possible.
  * Similarly, use a struct like `with std::pair` if possible.
  * `std::tuple` is almost never needed anywhere. Even when template metaprogramming, use of tuples can usually just be replaced with parameter packs.
  * Slow compile time.
* Avoid `if` statement initializers if variable scope isn't important.
  * Harder to find where a variable is declared. One would usually expect to see a line that starts with a type name or `auto`.
  * Just kinda ugly too.

## Appendix: Tips for commenting

Code should be self-documenting most of the time. Too many comments may indicate that the code can be improved.

Comments should...
* add additional information
* point out things that are difficult to catch
* usually start with a command sentence. Prefer "Does the thing" over "This does the thing"
* NOT describe what the code already describes
* NOT be a C++ tutorial (except for obscure features, eg: `std::popcount`)
* NOT document overall functionality of components in function docs or within code. Document this separately or as part of the class/struct. (eg: Invariants)


```cpp
// ❌ - C++ tutorial

// This is decided at compile time
if constexpr(condition) { ... }



// ❌ - describing code

// Checks if the ref count is zero
if (refCount == 0) { ... }



// ✔️ - Okay-ish

// No more refs, now destroy the instance
if (refCount == 0) { ... }



// ✔️ - additional information
int a = 20;
do_thing(a); // pass by reference



// ❌ - we know what these are

// Declare integers
int a = 20;
int b = 24;



// ❌ - we know what these are as well
/**
 * Struct that stores two ints
 */
struct IntPair
{
    int first; // The first int stored
    int second; // The second int stored
};



// ❌ - Teacher gives 10/10 for documentation. Useless noise in practice.
/**
 * @brief Check if the fish is dead
 *
 * @param fish [in] The fish that might be dead
 */
bool is_fish_dead(Fish const& fish) noexcept;



// ✔️ - no comment, function name is sufficient to know what it does
bool is_fish_dead(Fish const& fish) noexcept;



// ✔️ - New information that we might have not known
/**
 * Dead if health or hunger is zero
 */
bool is_fish_dead(Fish const& fish) noexcept;



// ❌ - Crucial information about invariants is hidden within a function
// 	(Move it to the class's documentation, and mention that it's an invariant!)

class DividedRow
{
public:
    /**
    * @brief Adds new items, reallocates if internal containers need to grow
    *
    * This makes sure that a divider is also created between each item. The first item added won't
    * create a new divider; no spacing is needed since there is only 1 item. The rest of the items
    * added will create a new divider.
    *
    * The number of dividers will always be "min(0, m_items.size() - 1)"
    */
    void push_item(Item newItem);

private:
    std::vector<Item> m_items;
    std::vector<Divider> m_dividers;
};

```
