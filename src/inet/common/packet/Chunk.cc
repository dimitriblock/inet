//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "inet/common/packet/Chunk.h"
#include "inet/common/packet/Serializer.h"

namespace inet {

bool Chunk::enableImplicitChunkSerialization = true;

Chunk::Chunk() :
    flags(0)
{
}

Chunk::Chunk(const Chunk& other) :
    flags(other.flags & ~FLAG_IMMUTABLE)
{
}

void Chunk::handleChange()
{
    assertMutable();
}

std::shared_ptr<Chunk> Chunk::createChunk(const std::type_info& typeInfo, const std::shared_ptr<Chunk>& chunk, int64_t offset, int64_t length)
{
    ByteOutputStream outputStream;
    serialize(outputStream, chunk, offset, length);
    ByteInputStream inputStream(outputStream.getBytes());
    return deserialize(inputStream, typeInfo);
}

ChunkSerializer *Chunk::createSerializer(const char *serializerClassName)
{
    if (serializerClassName == nullptr)
        throw cRuntimeError("Serializer class is unspecified");
    auto serializer = dynamic_cast<ChunkSerializer *>(omnetpp::createOne(serializerClassName));
    if (serializer == nullptr)
        throw cRuntimeError("Serializer class not found");
    return serializer;
}

void Chunk::serialize(ByteOutputStream& stream, const std::shared_ptr<Chunk>& chunk, int64_t offset, int64_t length)
{
    auto serializer = createSerializer(chunk->getSerializerClassName());
    serializer->serialize(stream, chunk, offset, length);
    delete serializer;
}

std::shared_ptr<Chunk> Chunk::deserialize(ByteInputStream& stream, const std::type_info& typeInfo)
{
    auto serializerClassName = std::string(opp_demangle_typename(typeInfo.name())) + std::string("Serializer");
    auto serializer = createSerializer(serializerClassName.c_str());
    auto chunk = serializer->deserialize(stream, typeInfo);
    delete serializer;
    if (stream.isReadBeyondEnd())
        chunk->makeIncomplete();
    return chunk;
}

std::shared_ptr<Chunk> Chunk::peek(const Iterator& iterator, int64_t length) const
{
    if (iterator.getPosition() == 0 && (length == -1 || length == getChunkLength()))
        return const_cast<Chunk *>(this)->shared_from_this();
    else
        return peek<SliceChunk>(iterator, length);
}

std::string Chunk::str() const
{
    std::ostringstream os;
    os << "Chunk, length = " << getChunkLength();
    return os.str();
}

} // namespace