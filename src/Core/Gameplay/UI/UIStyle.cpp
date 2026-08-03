#include "UIStyle.h"
#include "Renderer/Shader.h"

void UIStyle::ApplyTo(Shader* shader) const {
    if (!shader) return;
    shader->SetFloat("u_CornerRadius", cornerRadius);
    shader->SetVec4("u_BorderColor", border.r, border.g, border.b, border.a);
    shader->SetFloat("u_BorderWidth", borderWidth);
    shader->SetVec4("u_ShadowColor", shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a);
    shader->SetVec2("u_ShadowOffset", shadowOffset.x, shadowOffset.y);
    shader->SetFloat("u_ShadowBlur", shadowBlur);
}