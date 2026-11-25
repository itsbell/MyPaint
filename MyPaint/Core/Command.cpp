/**
 * @file Command.cpp
 * @brief 명령 직렬화 진입점
 */
#include "pch.h"
#include "Core/Command.h"

#include "Core/ByteStream.h"
#include "Core/FillCommand.h"
#include "Core/ImageCommands.h"
#include "Core/ShapeCommand.h"
#include "Core/StrokeCommand.h"

void WriteCommand(ByteWriter& writer, const Command& command)
{
    writer.Write(static_cast<uint16_t>(command.Type()));
    command.Write(writer);
}

std::unique_ptr<Command> ReadCommand(ByteReader& reader)
{
    const auto type = static_cast<CommandType>(reader.Read<uint16_t>());
    switch (type)
    {
    case CommandType::Stroke:
        return StrokeCommand::Read(reader);
    case CommandType::Fill:
        return FillCommand::Read(reader);
    case CommandType::Shape:
        return ShapeCommand::Read(reader);
    case CommandType::Patch:
        return PatchCommand::Read(reader);
    case CommandType::Invert:
        return InvertCommand::Read(reader);
    case CommandType::Transform:
        return TransformCommand::Read(reader);
    case CommandType::ResizeSkew:
        return ResizeSkewCommand::Read(reader);
    case CommandType::CanvasSize:
        return CanvasSizeCommand::Read(reader);
    }
    throw std::runtime_error("ReadCommand: 알 수 없는 명령 종류입니다.");
}
