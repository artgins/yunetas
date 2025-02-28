"""
sdata.py

SData - Structured Data

Copyright (c) 2025, ArtGins.
All Rights Reserved.
"""

from enum import IntEnum, IntFlag

##############################################################
#              Data Types (Enum)
##############################################################
class DataType(IntEnum):
    DTP_STRING  = 1
    DTP_BOOLEAN = 2
    DTP_INTEGER = 3
    DTP_REAL    = 4
    DTP_LIST    = 5
    DTP_DICT    = 6
    DTP_JSON    = 7
    DTP_POINTER = 8

DTP_SCHEMA = DataType.DTP_POINTER

##############################################################
#              SData Field Flags
##############################################################
class SDataFlag(IntFlag):
    SDF_NOTACCESS  = 0x00000001
    SDF_RD         = 0x00000002
    SDF_WR         = 0x00000004
    SDF_REQUIRED   = 0x00000008
    SDF_PERSIST    = 0x00000010
    SDF_VOLATIL    = 0x00000020
    SDF_RESOURCE   = 0x00000040
    SDF_PKEY       = 0x00000080
    SDF_STATS      = 0x00000800
    SDF_RSTATS     = 0x00002000
    SDF_PSTATS     = 0x00004000
    SDF_AUTHZ_R    = 0x00008000
    SDF_AUTHZ_W    = 0x00010000
    SDF_AUTHZ_X    = 0x00020000
    SDF_AUTHZ_P    = 0x00040000
    SDF_AUTHZ_S    = 0x00080000
    SDF_AUTHZ_RS   = 0x00100000

SDF_PUBLIC_ATTR = (SDataFlag.SDF_RD | SDataFlag.SDF_WR | SDataFlag.SDF_STATS |
                   SDataFlag.SDF_PERSIST | SDataFlag.SDF_VOLATIL |
                   SDataFlag.SDF_RSTATS | SDataFlag.SDF_PSTATS)

ATTR_WRITABLE   = SDataFlag.SDF_WR | SDataFlag.SDF_PERSIST
ATTR_READABLE   = SDF_PUBLIC_ATTR

##############################################################
#              SData Description Structure
##############################################################
class SDataDesc:
    def __init__(self, type, name, flag=0, default_value=None, description="",
                 alias=None, header=None, fillspace=0, json_fn=None, schema=None, authpth=None):
        self.type = type
        self.name = name
        self.flag = flag
        self.default_value = default_value
        self.description = description
        self.alias = alias
        self.header = header
        self.fillspace = fillspace
        self.json_fn = json_fn
        self.schema = schema
        self.authpth = authpth

##############################################################
#              SDATA Macros as Functions
##############################################################
def SDATA_END():
    return SDataDesc(0, None)

def SDATA(type, name, flag, default_value, description):
    return SDataDesc(type, name, flag, default_value, description)

def SDATACM(type, name, alias, items, json_fn, description):
    return SDataDesc(type, name, 0, None, description, alias, None, 0, json_fn, items, None)

def SDATACM2(type, name, flag, alias, items, json_fn, description):
    return SDataDesc(type, name, flag, None, description, alias, None, 0, json_fn, items, None)

def SDATAPM(type, name, flag, default_value, description):
    return SDataDesc(type, name, flag, default_value, description)

def SDATAAUTHZ(type, name, flag, alias, items, description):
    return SDataDesc(type, name, flag, None, description, alias, None, 0, None, items, None)

def SDATAPM0(type, name, flag, authpth, description):
    return SDataDesc(type, name, flag, None, description, None, None, 0, None, None, authpth)

def SDATADF(type, name, flag, header, fillspace, description):
    return SDataDesc(type, name, flag, None, description, None, header, fillspace)

##############################################################
#              Example Usage (Attributes)
##############################################################
attrs_table = [
SDATA(DataType.DTP_JSON,    "jobs",         SDataFlag.SDF_RD,         None,      "Jobs"),
SDATA(DataType.DTP_JSON,    "input_data",   SDataFlag.SDF_RD,         "{}",      "Input Jobs Data. Available in exec_action() and exec_result()"),
SDATA(DataType.DTP_JSON,    "output_data",  SDataFlag.SDF_RD,         "{}",      "Output Jobs Data. Available in exec_action() and exec_result()"),
SDATA(DataType.DTP_INTEGER, "timeout",      SDataFlag.SDF_PERSIST | SDataFlag.SDF_WR, "10000", "Action Timeout"),
SDATA(DataType.DTP_POINTER, "user_data",    0,                        None,      "User data"),
SDATA(DataType.DTP_POINTER, "subscriber",   0,                        None,      "Subscriber of output-events. Not a child gobj."),
SDATA_END()
]

# Exporting Definitions
__all__ = [
    "DataType", "SDataFlag", "SDF_PUBLIC_ATTR", "ATTR_WRITABLE", "ATTR_READABLE",
    "SDataDesc", "SDATA", "SDATACM", "SDATACM2", "SDATAPM", "SDATAAUTHZ", "SDATAPM0", "SDATADF", "SDATA_END"
]
