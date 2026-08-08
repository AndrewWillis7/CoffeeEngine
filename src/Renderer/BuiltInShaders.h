#pragma once

// RAW GLSL Source for the shaders the engine ships with
// Kept outside of asset files as this is abstracted away from
// the asset pipeline, (Maybe we can add these to the asset pipeline later for less bloat?)

namespace BuiltInShaders {
    extern const char* QuadVertexSrc;
    extern const char* FlatFragmentSrc;
    extern const char* GlowFragmentSrc;
    extern const char* RoundedPanelFragmentSrc;
    extern const char* TexturedFragmentSrc;
    extern const char* TextFragmentSrc;
    extern const char* BorderFragmentSrc;
} // End of Namespace BuiltInShaders