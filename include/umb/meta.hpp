#ifndef USCRIPT_MSGBUF_META_HPP
#define USCRIPT_MSGBUF_META_HPP

#pragma once

namespace umb::meta
{

enum class FieldType
{
    Boolean,
    Byte,
    Integer,
    Float,
    String,
    Bytes,
};

struct Field
{
    FieldType type;
};

}

#endif // USCRIPT_MSGBUF_META_HPP
