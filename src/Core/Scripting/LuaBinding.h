#pragma once
#include <new>
#include <string>
#include <type_traits>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace LuaBinding {

    // =====================================================================
    // Low-level primitives (unchanged from before the template layer
    // existed -- this is what every binding used to call by hand). Kept
    // public because the template layer below is built out of these, and
    // any binding that needs to drop to metal still can.
    // =====================================================================

    template <typename T>
    struct PtrUserdata
    {
        T* ptr;
    };

    template <typename T>
    void RegisterMetatable(lua_State* L, const char* metatableName, const luaL_Reg* methods) {
        if (luaL_newmetatable(L, metatableName)) {
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
            luaL_setfuncs(L, methods, 0);
        }
        lua_pop(L, 1);
    }

    template <typename T>
    void PushPtr(lua_State* L, const char* metatableName, T* value) {
        auto* ud = static_cast<PtrUserdata<T>*>(lua_newuserdatauv(L, sizeof(PtrUserdata<T>), 0));
        ud->ptr = value;
        luaL_setmetatable(L, metatableName);
    }

    template <typename T>
    T* CheckPtr(lua_State* L, int index, const char* metatableName) {
        return static_cast<PtrUserdata<T>*>(luaL_checkudata(L, index, metatableName))->ptr;
    }

    // --- Value Types (LUA constructs and owns the instance, e.g. Vector2) ---

    template <typename T>
    void RegisterValueMetatable(lua_State* L, const char* metatableName, const luaL_Reg* methods) {
        if (luaL_newmetatable(L, metatableName)) {
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
            luaL_setfuncs(L, methods, 0);

            lua_pushcfunction(L, [](lua_State* gcL) -> int {
                static_cast<T*>(lua_touserdata(gcL, 1))->~T();
                return 0;
            });
            lua_setfield(L, -2, "__gc");
        }
        lua_pop(L, 1);
    }

    template <typename T>
    void PushNew(lua_State* L, const char* metatableName, T value) {
        void* mem = lua_newuserdatauv(L, sizeof(T), 0);
        new (mem) T(value);
        luaL_setmetatable(L, metatableName);
    }

    template <typename T>
    T* CheckValue(lua_State* L, int index, const char* metatableName) {
        return static_cast<T*>(luaL_checkudata(L, index, metatableName));
    }

    // =====================================================================
    // Type registry -- one line per bound type, written once next to the
    // type's own Register() function. Everything below reads these two
    // traits instead of a copy-pasted `kMetatableName` string in every
    // file that happens to touch the type.
    // =====================================================================

    // Specialize per bound type: struct MetatableOf<Shader> { static constexpr const char* name = "Coffee.Shader"; };
    template <typename T> struct MetatableOf;

    // Specialize to std::true_type for value types (Lua owns the instance,
    // e.g. Vector2). Left false (pointer type, engine owns the instance,
    // Lua just holds a pointer) for everything else.
    template <typename T> struct IsValueType : std::false_type {};

    template <typename T, typename = void>
    struct HasMetatable : std::false_type {};
    template <typename T>
    struct HasMetatable<T, std::void_t<decltype(MetatableOf<T>::name)>> : std::true_type {};

    // Fetches `self` off the stack the right way for either kind of bound type.
    template <typename T>
    T* GetSelf(lua_State* L, int index) {
        if constexpr (IsValueType<T>::value)
            return CheckValue<T>(L, index, MetatableOf<T>::name);
        else
            return CheckPtr<T>(L, index, MetatableOf<T>::name);
    }

    // =====================================================================
    // Value<T> -- stack <-> C++ conversion for one argument/return slot.
    // This is the part that actually erases the marshaling boilerplate:
    // every `static_cast<float>(luaL_checknumber(L, n))` line collapses
    // into one specialization written once, used everywhere.
    // =====================================================================

    template <typename T> struct Value;

    template <> struct Value<float> {
        static float Get(lua_State* L, int idx) { return static_cast<float>(luaL_checknumber(L, idx)); }
        static void Push(lua_State* L, float v) { lua_pushnumber(L, v); }
    };
    template <> struct Value<double> {
        static double Get(lua_State* L, int idx) { return luaL_checknumber(L, idx); }
        static void Push(lua_State* L, double v) { lua_pushnumber(L, v); }
    };
    template <> struct Value<int> {
        static int Get(lua_State* L, int idx) { return static_cast<int>(luaL_checkinteger(L, idx)); }
        static void Push(lua_State* L, int v) { lua_pushinteger(L, v); }
    };
    template <> struct Value<bool> {
        static bool Get(lua_State* L, int idx) { return lua_toboolean(L, idx) != 0; }
        static void Push(lua_State* L, bool v) { lua_pushboolean(L, v); }
    };
    template <> struct Value<const char*> {
        static const char* Get(lua_State* L, int idx) { return luaL_checkstring(L, idx); }
        static void Push(lua_State* L, const char* v) { lua_pushstring(L, v); }
    };
    template <> struct Value<std::string> {
        static std::string Get(lua_State* L, int idx) { return luaL_checkstring(L, idx); }
        static void Push(lua_State* L, const std::string& v) { lua_pushstring(L, v.c_str()); }
    };

    // Pointer to any registered bound type: RigidBody2D*, Shader*, etc.
    // Push nil-checks automatically -- a null Shader* becomes Lua nil,
    // not a dangling userdata, so callers can `if not body:GetShader() then`.
    template <typename T> struct Value<T*> {
        static T* Get(lua_State* L, int idx) { return CheckPtr<T>(L, idx, MetatableOf<T>::name); }
        static void Push(lua_State* L, T* v) {
            if (!v) lua_pushnil(L);
            else PushPtr<T>(L, MetatableOf<T>::name, v);
        }
    };

    // Registered value type (e.g. Vector2): Lua owns a fresh copy.
    // Only participates when the type opted in via IsValueType<T>.
    template <typename T>
    struct ValueTypeConverter {
        static T Get(lua_State* L, int idx) { return *CheckValue<T>(L, idx, MetatableOf<T>::name); }
        static void Push(lua_State* L, T v) { PushNew<T>(L, MetatableOf<T>::name, v); }
    };

    // =====================================================================
    // Argument extraction. Handles the by-reference cases (const Foo&,
    // Foo&) that show up in real signatures like
    // RigidBody2D::ResolveCollisionWith(RigidBody2D& other) --
    // dereferences a fetched pointer instead of copying the object.
    // =====================================================================

    template <typename Arg>
    decltype(auto) Extract(lua_State* L, int idx) {
        using Bare = std::remove_cvref_t<Arg>;
        if constexpr (std::is_pointer_v<Bare>) {
            using Pointee = std::remove_pointer_t<Bare>;
            return Value<Pointee*>::Get(L, idx);
        } else if constexpr (std::is_reference_v<Arg> && HasMetatable<Bare>::value && !IsValueType<Bare>::value) {
            // Reference to an engine pointer-owned type (RigidBody2D&, etc.)
            // -- fetch the pointer Lua actually holds and deref it, no copy.
            return *Value<Bare*>::Get(L, idx);
        } else if constexpr (HasMetatable<Bare>::value && IsValueType<Bare>::value) {
            return ValueTypeConverter<Bare>::Get(L, idx);
        } else {
            return Value<Bare>::Get(L, idx);
        }
    }

    template <typename Ret>
    void PushResult(lua_State* L, Ret&& v) {
        using Bare = std::remove_cvref_t<Ret>;
        if constexpr (HasMetatable<Bare>::value && IsValueType<Bare>::value)
            ValueTypeConverter<Bare>::Push(L, std::forward<Ret>(v));
        else
            Value<Bare>::Push(L, std::forward<Ret>(v));
    }

    // =====================================================================
    // Member-function trampoline: given a pointer-to-member-function,
    // synthesizes the `int(lua_State*)` C function Lua actually calls.
    // `self` is stack slot 1, args start at slot 2 -- the normal
    // `obj:Method(a, b)` calling convention.
    // =====================================================================

    template <auto Fn> struct MemberFnTraits;

    template <typename Ret, typename Class, typename... Args, Ret(Class::*Fn)(Args...)>
    struct MemberFnTraits<Fn> {
        template <size_t... I>
        static int CallImpl(lua_State* L, Class* self, std::index_sequence<I...>) {
            if constexpr (std::is_void_v<Ret>) {
                (self->*Fn)(Extract<Args>(L, 2 + static_cast<int>(I))...);
                return 0;
            } else {
                PushResult<Ret>(L, (self->*Fn)(Extract<Args>(L, 2 + static_cast<int>(I))...));
                return 1;
            }
        }
        static int Call(lua_State* L) {
            Class* self = GetSelf<Class>(L, 1);
            return CallImpl(L, self, std::index_sequence_for<Args...>{});
        }
    };

    // const-qualified overload (Vector2's operators, CollidesWith, etc.)
    template <typename Ret, typename Class, typename... Args, Ret(Class::*Fn)(Args...) const>
    struct MemberFnTraits<Fn> {
        template <size_t... I>
        static int CallImpl(lua_State* L, Class* self, std::index_sequence<I...>) {
            if constexpr (std::is_void_v<Ret>) {
                (self->*Fn)(Extract<Args>(L, 2 + static_cast<int>(I))...);
                return 0;
            } else {
                PushResult<Ret>(L, (self->*Fn)(Extract<Args>(L, 2 + static_cast<int>(I))...));
                return 1;
            }
        }
        static int Call(lua_State* L) {
            Class* self = GetSelf<Class>(L, 1);
            return CallImpl(L, self, std::index_sequence_for<Args...>{});
        }
    };

    // Plain function pointer / static member function -- no `self` at all,
    // every Lua arg (including one passed via `:` colon-call sugar, e.g.
    // Vector2::Distance registered as an instance method) starts at slot 1.
    template <auto Fn> struct FreeFnTraits;

    template <typename Ret, typename... Args, Ret(*Fn)(Args...)>
    struct FreeFnTraits<Fn> {
        template <size_t... I>
        static int CallImpl(lua_State* L, std::index_sequence<I...>) {
            if constexpr (std::is_void_v<Ret>) {
                Fn(Extract<Args>(L, 1 + static_cast<int>(I))...);
                return 0;
            } else {
                PushResult<Ret>(L, Fn(Extract<Args>(L, 1 + static_cast<int>(I))...));
                return 1;
            }
        }
        static int Call(lua_State* L) { return CallImpl(L, std::index_sequence_for<Args...>{}); }
    };

    // Dispatches to whichever of the two shapes above matches `Fn` --
    // Class<T>::Method<> and Table::Raw-registered free functions both
    // go through this, so callers don't need to know or care which kind
    // of function pointer they handed in.
    template <auto Fn>
    int Wrap(lua_State* L) {
        if constexpr (std::is_member_function_pointer_v<decltype(Fn)>)
            return MemberFnTraits<Fn>::Call(L);
        else
            return FreeFnTraits<Fn>::Call(L);
    }

    // =====================================================================
    // Upvalue-bound free function: `self` comes from lua_upvalueindex(1)
    // (a captured engine context, e.g. ActorRegistry*) instead of stack
    // slot 1. Args start at slot 1. This is the shape every `.new(...)`
    // factory and every free-function-with-captured-context binding uses.
    // =====================================================================

    template <auto Fn> struct UpvalueFnTraits;

    template <typename Ret, typename Class, typename... Args, Ret(Class::*Fn)(Args...)>
    struct UpvalueFnTraits<Fn> {
        template <size_t... I>
        static int CallImpl(lua_State* L, Class* self, std::index_sequence<I...>) {
            if constexpr (std::is_void_v<Ret>) {
                (self->*Fn)(Extract<Args>(L, 1 + static_cast<int>(I))...);
                return 0;
            } else {
                PushResult<Ret>(L, (self->*Fn)(Extract<Args>(L, 1 + static_cast<int>(I))...));
                return 1;
            }
        }
        static int Call(lua_State* L) {
            auto* self = static_cast<Class*>(lua_touserdata(L, lua_upvalueindex(1)));
            luaL_argcheck(L, self != nullptr, 1, "engine context not bound");
            return CallImpl(L, self, std::index_sequence_for<Args...>{});
        }
    };

    template <typename Ret, typename Class, typename... Args, Ret(Class::*Fn)(Args...) const>
    struct UpvalueFnTraits<Fn> {
        template <size_t... I>
        static int CallImpl(lua_State* L, Class* self, std::index_sequence<I...>) {
            if constexpr (std::is_void_v<Ret>) {
                (self->*Fn)(Extract<Args>(L, 1 + static_cast<int>(I))...);
                return 0;
            } else {
                PushResult<Ret>(L, (self->*Fn)(Extract<Args>(L, 1 + static_cast<int>(I))...));
                return 1;
            }
        }
        static int Call(lua_State* L) {
            auto* self = static_cast<Class*>(lua_touserdata(L, lua_upvalueindex(1)));
            luaL_argcheck(L, self != nullptr, 1, "engine context not bound");
            return CallImpl(L, self, std::index_sequence_for<Args...>{});
        }
    };

    template <auto Fn>
    int WrapUpvalue(lua_State* L) { return UpvalueFnTraits<Fn>::Call(L); }

    // Bare-global variants of the upvalue pattern above -- for engine
    // globals that aren't table members, e.g. `SetClearColor(r,g,b)`
    // rather than `Graphics.SetClearColor(r,g,b)`.
    template <auto Fn, typename Ctx>
    void BindFunction(lua_State* L, const char* name, Ctx* ctx) {
        lua_pushlightuserdata(L, ctx);
        lua_pushcclosure(L, &WrapUpvalue<Fn>, 1);
        lua_setglobal(L, name);
    }

    template <typename Ctx>
    void BindRawFunction(lua_State* L, const char* name, Ctx* ctx, lua_CFunction fn) {
        lua_pushlightuserdata(L, ctx);
        lua_pushcclosure(L, fn, 1);
        lua_setglobal(L, name);
    }

    // =====================================================================
    // Field properties: direct member-data access, no hand-written getter/
    // setter needed at all. Covers the "GetX/SetX just reads/writes a
    // field" case, which in practice is most of them.
    // =====================================================================

    // Plain scalar/bool/string field -- RigidBody2D::mass, Shader::overdrawScale, ...
    template <auto Field> struct FieldTraits;
    template <typename T, typename Class, T Class::*Field>
    struct FieldTraits<Field> {
        static int Get(lua_State* L) { PushResult(L, GetSelf<Class>(L, 1)->*Field); return 1; }
        static int Set(lua_State* L) { GetSelf<Class>(L, 1)->*Field = Extract<T>(L, 2); return 0; }
    };

    // Scalar field crossing the Lua boundary with a unit conversion, e.g.
    // RigidBody2D::angularVelocity (stored radians, scripted in degrees).
    template <auto Field, float ToLua, float ToNative> struct ScaledFieldTraits;
    template <typename Class, float Class::*Field, float ToLua, float ToNative>
    struct ScaledFieldTraits<Field, ToLua, ToNative> {
        static int Get(lua_State* L) { lua_pushnumber(L, GetSelf<Class>(L, 1)->*Field * ToLua); return 1; }
        static int Set(lua_State* L) { GetSelf<Class>(L, 1)->*Field = static_cast<float>(luaL_checknumber(L, 2)) * ToNative; return 0; }
    };

    // Direct Vector2 field, crossed as two raw numbers (x, y) rather than
    // a Vector2 userdata -- matches the existing hot-path convention for
    // GetPosition/GetVelocity (called every frame; avoids an alloc+GC).
    template <auto Field> struct Vec2FieldTraits;
    template <typename Class, typename Vec2T, Vec2T Class::*Field>
    struct Vec2FieldTraits<Field> {
        static int Get(lua_State* L) {
            Vec2T& v = GetSelf<Class>(L, 1)->*Field;
            lua_pushnumber(L, v.x);
            lua_pushnumber(L, v.y);
            return 2;
        }
        static int Set(lua_State* L) {
            Vec2T& v = GetSelf<Class>(L, 1)->*Field;
            v.x = static_cast<float>(luaL_checknumber(L, 2));
            v.y = static_cast<float>(luaL_checknumber(L, 3));
            return 0;
        }
    };

    // Direct non-owning pointer field (Shader*, CollisionShape2D*, ...)
    // with nil-clears-it setter semantics -- matches SetShader(nil),
    // SetCollisionShape(nil), SetPlayerConfig(nil) throughout the engine.
    template <auto Field> struct PtrFieldTraits;
    template <typename T, typename Class, T* Class::*Field>
    struct PtrFieldTraits<Field> {
        static int Get(lua_State* L) { Value<T*>::Push(L, GetSelf<Class>(L, 1)->*Field); return 1; }
        static int Set(lua_State* L) {
            Class* self = GetSelf<Class>(L, 1);
            if (lua_isnoneornil(L, 2)) self->*Field = nullptr;
            else self->*Field = Value<T*>::Get(L, 2);
            return 0;
        }
    };

    // =====================================================================
    // Fluent builders. This is the ::AddToLuaEnvironment() idea realized
    // in C++: one line per bound function/property, appended together.
    // =====================================================================

    // Binds a metatable (a class's methods + properties) for type T.
    template <typename T>
    class Class {
    public:
        Class(lua_State* L, const char* metatableName) : m_L(L), m_Name(metatableName) {}

        template <auto Fn>
        Class& Method(const char* name) { Push(name, &Wrap<Fn>); return *this; }

        template <auto Field>
        Class& Property(const char* getName, const char* setName) {
            Push(getName, &FieldTraits<Field>::Get);
            Push(setName, &FieldTraits<Field>::Set);
            return *this;
        }

        template <auto Field, float ToLua, float ToNative>
        Class& ScaledProperty(const char* getName, const char* setName) {
            Push(getName, &ScaledFieldTraits<Field, ToLua, ToNative>::Get);
            Push(setName, &ScaledFieldTraits<Field, ToLua, ToNative>::Set);
            return *this;
        }

        template <auto Field>
        Class& Vec2Property(const char* getName, const char* setName) {
            Push(getName, &Vec2FieldTraits<Field>::Get);
            Push(setName, &Vec2FieldTraits<Field>::Set);
            return *this;
        }

        template <auto Field>
        Class& PtrProperty(const char* getName, const char* setName) {
            Push(getName, &PtrFieldTraits<Field>::Get);
            Push(setName, &PtrFieldTraits<Field>::Set);
            return *this;
        }

        // Escape hatch for the handful of cases that don't fit the
        // mechanical shapes above (optional args, custom formatting,
        // operand-order-dependent metamethods like __mul).
        Class& Raw(const char* name, lua_CFunction fn) { Push(name, fn); return *this; }

        void Finish() {
            m_Methods.push_back({nullptr, nullptr});
            if constexpr (IsValueType<T>::value)
                RegisterValueMetatable<T>(m_L, m_Name, m_Methods.data());
            else
                RegisterMetatable<T>(m_L, m_Name, m_Methods.data());
        }

    private:
        void Push(const char* name, lua_CFunction fn) { m_Methods.push_back({name, fn}); }

        lua_State* m_L;
        const char* m_Name;
        std::vector<luaL_Reg> m_Methods;
    };

    // Binds a global table (Physics.SetGravity, RigidBody2D.new, Actors.Dump, ...).
    class Table {
    public:
        explicit Table(lua_State* L) : m_L(L) { lua_newtable(L); }

        // Free/static function, no captured context (Physics.SetGravity, Vector2.new).
        Table& Raw(const char* name, lua_CFunction fn) {
            lua_pushcfunction(m_L, fn);
            lua_setfield(m_L, -2, name);
            return *this;
        }

        // Member function called on a captured context pointer, e.g.
        // ActorRegistry::CreateRigidBody bound as RigidBody2D.new(...).
        template <auto Fn, typename Ctx>
        Table& Function(const char* name, Ctx* ctx) {
            return RawWithContext(name, ctx, &WrapUpvalue<Fn>);
        }

        // Escape hatch for hand-written trampolines that still need a
        // captured context as upvalue(1) -- e.g. a `.new()` with optional
        // args the generic Function<> can't express (see RigidBody2D.new).
        template <typename Ctx>
        Table& RawWithContext(const char* name, Ctx* ctx, lua_CFunction fn) {
            lua_pushlightuserdata(m_L, ctx);
            lua_pushcclosure(m_L, fn, 1);
            lua_setfield(m_L, -2, name);
            return *this;
        }

        Table& Constant(const char* name, int value) {
            lua_pushinteger(m_L, value);
            lua_setfield(m_L, -2, name);
            return *this;
        }

        void Finish(const char* globalName) { lua_setglobal(m_L, globalName); }

    private:
        lua_State* m_L;
    };

} // End Namespace LuaBinding