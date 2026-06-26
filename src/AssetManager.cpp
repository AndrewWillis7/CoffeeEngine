#include "AssetManager.h"

bool AssetManager::LoadSprite(const std::wstring& key,
                              const std::wstring& path,
                              int fw, int fh, int cols, int rows)
{
    auto sprite = std::make_unique<Sprite>(path.c_str(), fw, fh, cols, rows);

    if (!sprite) return false;

    m_sprites[key] = std::move(sprite);
    return true;
}

Sprite* AssetManager::GetSprite(const std::wstring& key) {
    auto it = m_sprites.find(key);
    if (it != m_sprites.end())
        return it->second.get();
    
    return nullptr;
}