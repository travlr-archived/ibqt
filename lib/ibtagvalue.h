// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#ifndef IBTAGVALUE_H
#define IBTAGVALUE_H

#include <QByteArray>

struct TagValue
{
    TagValue()
    {}

    TagValue(const QByteArray & tag, const QByteArray & value)
        : tag(tag)
        , value(value) {}

    QByteArray tag;
    QByteArray value;
};

#endif // IBTAGVALUE_H

