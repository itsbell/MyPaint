/**
 * @file Tool.cpp
 * @brief Tool 기본 구현
 */
#include "pch.h"
#include "Tools/Tool.h"

#include "Core/Command.h"

void Tool::DrawPreview(Surface& surface) const
{
    if (const Command* preview = PreviewCommand())
    {
        preview->Execute(surface);
    }
}
