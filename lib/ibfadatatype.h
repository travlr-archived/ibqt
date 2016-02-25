// Copyright (C) 2016  ProDataLab, Peter Alexander
// GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// http://www.gnu.org/licenses/gpl-3.0-standalone.html

#ifndef IBFADATATYPE_H
#define IBFADATATYPE_H

enum FaDataType { GROUPS=1, PROFILES, ALIASES } ;

inline const char* faDataTypeStr ( FaDataType pFaDataType )
{
    switch (pFaDataType) {
        case GROUPS:
            return "GROUPS" ;
            break ;
        case PROFILES:
            return "PROFILES" ;
            break ;
        case ALIASES:
            return "ALIASES" ;
            break ;
    }
    return 0 ;
}

#endif // IBFADATATYPE_H

