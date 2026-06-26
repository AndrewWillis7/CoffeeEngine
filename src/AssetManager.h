#pragma once
#include <unordered_map>
#include <memory>
#include <string>

#include "spriteload.h"

class AssetManager {
public:
    bool LoadSprite(const std::wstring& key,
                    const std::wstring& path,
                    int fw, int fh, int cols, int rows);
    
    Sprite* GetSprite(const std::wstring& key);

private:
    std::unordered_map<std::wstring, std::unique_ptr<Sprite>> m_sprites;
};